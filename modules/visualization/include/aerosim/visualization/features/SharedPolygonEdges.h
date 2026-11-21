#pragma once

#include <vtkCellArray.h>
#include <vtkSmartPointer.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class vtkPolyData;

namespace aerosim { namespace visualization {

// One incident face owns each unordered pair of source point IDs. A bit
// identifies its outgoing edge in VTK 8.2's original polygon connectivity.
class SharedPolygonEdges {
public:
    using BatchConsumer = std::function<void(const std::uint32_t*, std::size_t)>;

    // Returns empty when the native mapper must handle this dataset (e.g.
    // strips or explicit edge flags). A single volume's extraction may supply
    // its vtkOriginalPointIds array to match adjacent faces across split vertices.
    static std::shared_ptr<const SharedPolygonEdges> build(
        vtkPolyData* mesh, const std::atomic_bool* cancelled = nullptr,
        vtkIdTypeArray* sourcePointIds = nullptr, vtkIdType sourcePointCount = VTK_ID_MAX);
    bool matches(vtkPolyData* mesh) const;
    std::uint64_t edgeCount() const { return edgeCount_; }
    // Count passed to the consumer is the number of uint32 indices, two per edge.
    void forEachBatch(vtkPolyData* mesh, const BatchConsumer& consume) const;

private:
    vtkSmartPointer<vtkCellArray> polygons_;
    vtkSmartPointer<vtkIdTypeArray> sourcePointIds_;
    vtkMTimeType sourceIdsTime_ = 0;
    vtkIdTypeArray* data_ = nullptr;
    vtkMTimeType polygonsTime_ = 0, dataTime_ = 0;
    vtkIdType points_ = 0, cells_ = 0, entries_ = 0;
    std::uint64_t edgeCount_ = 0;
    std::vector<std::uint64_t> owners_;
};

} }
