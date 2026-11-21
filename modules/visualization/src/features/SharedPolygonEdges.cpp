#include "aerosim/visualization/features/SharedPolygonEdges.h"
#include <vtkDataSetAttributes.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace aerosim { namespace visualization {
namespace {

constexpr std::size_t maxSlots = 1u << 23; // At most 64 MiB of temporary edge keys.
constexpr vtkIdType facesPerBlock = 1024;
using Key = std::uint64_t;

struct Block {
    vtkIdType begin, end;
    std::uint64_t bands;
};

void checkCancel(const std::atomic_bool* cancel) {
    if (cancel && cancel->load()) throw std::runtime_error("已取消导入");
}

bool supported(vtkPolyData* mesh) {
    return mesh && mesh->GetNumberOfPolys() > 0 && mesh->GetNumberOfPoints() > 0
        && mesh->GetNumberOfPolys() == mesh->GetNumberOfCells()
        && static_cast<Key>(mesh->GetNumberOfPoints() - 1) <= (std::numeric_limits<std::uint32_t>::max)()
        && !mesh->GetPointData()->GetAttribute(vtkDataSetAttributes::EDGEFLAG);
}

Key edgeKey(vtkIdType a, vtkIdType b) {
    return (static_cast<Key>((std::min)(a, b)) << 32) | static_cast<Key>((std::max)(a, b));
}

// A permutation of 64-bit integers. High bits can partition even a single
// exceptionally crowded endpoint band without losing distinct topology.
Key mix(Key k) {
    k = (k ^ (k >> 30)) * 0xbf58476d1ce4e5b9ULL;
    k = (k ^ (k >> 27)) * 0x94d049bb133111ebULL;
    return k ^ (k >> 31);
}

class EdgeBuilder {
public:
    EdgeBuilder(const vtkIdType* records, vtkIdType pointCount, vtkIdType entries,
                const vtkIdType* sourceIds, vtkIdType largestKeyId,
                std::uint64_t* owners, const std::atomic_bool* cancel)
        : records_(records), sourceIds_(sourceIds), points_(pointCount), owners_(owners), cancel_(cancel) {
        unsigned idBits = 0;
        for (auto id = static_cast<Key>(largestKeyId); id; id >>= 1) ++idBits;
        unsigned bandBits = 0;
        while (bandBits < 6 && bandBits < idBits &&
               static_cast<Key>(entries) > (maxSlots / 2) * (Key(1) << bandBits)) ++bandBits;
        bandShift_ = idBits - bandBits;
    }

    bool indexBlocks(vtkIdType faceCount, vtkIdType entries) {
        vtkIdType offset = 0;
        for (vtkIdType face = 0; face < faceCount; ) {
            checkCancel(cancel_);
            Block block{offset, 0, 0};
            const auto count = (std::min)(facesPerBlock, faceCount - face);
            for (vtkIdType i = 0; i < count; ++i) {
                if (offset >= entries) return false;
                const auto n = records_[offset++];
                if (n < 3 || n > entries - offset) return false;
                const auto end = offset + n;
                const auto last = records_[end - 1];
                if (last < 0 || last >= points_) return false;
                auto previous = sourceIds_ ? sourceIds_[last] : last;
                for (; offset < end; ++offset) {
                    const auto point = records_[offset];
                    if (point < 0 || point >= points_) return false;
                    const auto current = sourceIds_ ? sourceIds_[point] : point;
                    if (current == previous) return false;
                    const auto band = static_cast<Key>((std::min)(previous, current)) >> bandShift_;
                    ++incidences_[band];
                    block.bands |= Key(1) << band;
                    previous = current;
                }
            }
            block.end = offset;
            blocks_.push_back(block);
            face += count;
        }
        return offset == entries;
    }

    std::uint64_t markOwners() {
        for (unsigned band = 0; band < incidences_.size(); ++band) {
            const auto count = incidences_[band];
            if (!count) continue;
            std::size_t capacity = 4;
            while (capacity < maxSlots && capacity - capacity / 4 < count) capacity *= 2;
            std::vector<Key> table(capacity);
            markBand(band, 0, 0, table);
        }
        return distinct_;
    }

private:
    void markBand(unsigned band, unsigned bits, Key prefix, std::vector<Key>& table) {
        std::fill(table.begin(), table.end(), Key(0));
        const auto size = table.size();
        auto* keys = table.data();
        std::size_t used = 0;
        for (const auto& block : blocks_) {
            if (!(block.bands & (Key(1) << band))) continue;
            checkCancel(cancel_);
            for (auto pos = block.begin; pos < block.end; ) {
                const auto n = records_[pos++];
                const auto end = pos + n;
                // Handle the closing edge and all successive endpoint pairs.
                auto from = end - 1;
                for (; pos < end; ++pos) {
                    const auto a = records_[from], b = records_[pos];
                    const auto key = sourceIds_ ? edgeKey(sourceIds_[a], sourceIds_[b]) : edgeKey(a, b);
                    if (((key >> 32) >> bandShift_) == band) {
                        const auto hash = mix(key);
                        if (!bits || (hash >> (64 - bits)) == prefix) {
                            auto slot = static_cast<std::size_t>(hash) & (size - 1);
                            while (keys[slot] && keys[slot] != key) slot = (slot + 1) & (size - 1);
                            if (!keys[slot]) {
                                if (used == size - size / 4) {
                                    // Split a skewed band by hash prefix, reusing the same table.
                                    // A retry visits faces in the original order: an ownership
                                    // bit already set is still the first incidence of that edge.
                                    markBand(band, bits + 1, prefix << 1, table);
                                    markBand(band, bits + 1, (prefix << 1) | 1, table);
                                    return;
                                }
                                keys[slot] = key;
                                ++used;
                                auto& word = owners_[from >> 6];
                                const auto bit = Key(1) << (from & 63);
                                if (!(word & bit)) { word |= bit; ++distinct_; }
                            }
                        }
                    }
                    from = pos;
                }
            }
        }
    }

    const vtkIdType* records_;
    const vtkIdType* sourceIds_;
    vtkIdType points_;
    std::uint64_t* owners_;
    const std::atomic_bool* cancel_;
    unsigned bandShift_ = 0;
    std::array<Key, 64> incidences_{};
    std::vector<Block> blocks_;
    Key distinct_ = 0;
};

} // namespace

std::shared_ptr<const SharedPolygonEdges> SharedPolygonEdges::build(
    vtkPolyData* mesh, const std::atomic_bool* cancelled,
    vtkIdTypeArray* sourcePointIds, vtkIdType sourcePointCount) {
    checkCancel(cancelled);
    if (!supported(mesh)) return {};
    auto* polys = mesh->GetPolys();
    const auto entries = polys->GetNumberOfConnectivityEntries();
    if (entries <= 0 || mesh->GetNumberOfPolys() > entries / 4) return {};
    const auto pointCount = mesh->GetNumberOfPoints();
    auto largestKeyId = pointCount - 1;
    const vtkIdType* sourceIds = nullptr;
    if (sourcePointIds) {
        if (sourcePointIds != mesh->GetPointData()->GetArray("vtkOriginalPointIds") ||
            sourcePointIds->GetNumberOfComponents() != 1 || sourcePointIds->GetNumberOfTuples() != pointCount)
            return {};
        sourceIds = sourcePointIds->GetPointer(0);
        largestKeyId = 0;
        for (vtkIdType i = 0; i < pointCount; ++i) {
            if ((i & 65535) == 0) checkCancel(cancelled);
            const auto id = sourceIds[i];
            if (id < 0 || id >= sourcePointCount || static_cast<Key>(id) > (std::numeric_limits<std::uint32_t>::max)())
                return {};
            largestKeyId = (std::max)(largestKeyId, id);
        }
    }
    std::shared_ptr<SharedPolygonEdges> result(new SharedPolygonEdges);
    result->owners_.resize((static_cast<Key>(entries) + 63) / 64);
    const auto* records = polys->GetPointer();
    if (!records) return {};
    EdgeBuilder builder(records, pointCount, entries, sourceIds, largestKeyId, result->owners_.data(), cancelled);
    if (!builder.indexBlocks(mesh->GetNumberOfPolys(), entries)) return {};
    result->edgeCount_ = builder.markOwners();
    result->polygons_ = polys;
    result->data_ = polys->GetData();
    result->polygonsTime_ = polys->GetMTime();
    result->dataTime_ = result->data_->GetMTime();
    result->sourcePointIds_ = sourcePointIds;
    if (sourcePointIds) result->sourceIdsTime_ = sourcePointIds->GetMTime();
    result->points_ = mesh->GetNumberOfPoints();
    result->cells_ = mesh->GetNumberOfPolys();
    result->entries_ = entries;
    return result;
}

bool SharedPolygonEdges::matches(vtkPolyData* mesh) const {
    return supported(mesh) && mesh->GetPolys() == polygons_.GetPointer()
        && mesh->GetNumberOfPoints() == points_ && mesh->GetNumberOfPolys() == cells_
        && polygons_->GetNumberOfConnectivityEntries() == entries_
        && polygons_->GetData() == data_ && polygons_->GetMTime() == polygonsTime_
        && data_->GetMTime() == dataTime_
        && (!sourcePointIds_ || (sourcePointIds_.GetPointer() == mesh->GetPointData()->GetArray("vtkOriginalPointIds")
                                && sourcePointIds_->GetMTime() == sourceIdsTime_));
}

void SharedPolygonEdges::forEachBatch(vtkPolyData* mesh, const BatchConsumer& consume) const {
    if (!matches(mesh)) throw std::invalid_argument("Polygon edge topology has changed");
    std::vector<std::uint32_t> batch(32768);
    auto* indices = batch.data();
    const auto* owners = owners_.data();
    const auto* records = polygons_->GetPointer();
    std::size_t used = 0;
    for (vtkIdType pos = 0; pos < entries_; ) {
        const auto n = records[pos++];
        const auto end = pos + n;
        auto from = end - 1;
        for (; pos < end; ++pos) {
            if (owners[from >> 6] & (Key(1) << (from & 63))) {
                const auto a = static_cast<std::uint32_t>(records[from]);
                const auto b = static_cast<std::uint32_t>(records[pos]);
                indices[used++] = (std::min)(a, b);
                indices[used++] = (std::max)(a, b);
                if (used == batch.size()) { consume(indices, used); used = 0; }
            }
            from = pos;
        }
    }
    if (used) consume(indices, used);
}

} }
