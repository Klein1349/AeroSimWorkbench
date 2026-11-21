#include "aerosim/visualization/scene/BoundaryPolyDataMapper.h"
#include "aerosim/visualization/features/SharedPolygonEdges.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLIndexBufferObject.h>
#include <vtkOpenGLVertexBufferObject.h>
#include <vtkOpenGLVertexBufferObjectGroup.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aerosim { namespace visualization {
namespace {

void discard(vtkOpenGLIndexBufferObject* buffer) {
    buffer->IndexCount = 0;
    buffer->ReleaseGraphicsResources();
}

// Write through the copy target so the currently bound vertex array's
// element binding is unaffected. VTK retains ownership of the GL name.
class IndexUpload {
public:
    IndexUpload(QOpenGLExtraFunctions* gl, vtkOpenGLIndexBufferObject* target, std::size_t capacity)
        : gl_(gl), target_(target), capacity_(capacity) {
        target_->IndexCount = 0;
        if (!target_->GenerateBuffer(vtkOpenGLBufferObject::ElementArrayBuffer))
            throw std::runtime_error("Cannot create polygon index buffer");
        gl_->glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &previous_);
        gl_->glBindBuffer(GL_COPY_WRITE_BUFFER, target_->GetHandle());
    }
    ~IndexUpload() { gl_->glBindBuffer(GL_COPY_WRITE_BUFFER, previous_); }

    void allocate() {
        const auto bytes = static_cast<qopengl_GLsizeiptr>(capacity_ * sizeof(std::uint32_t));
        gl_->glBufferData(GL_COPY_WRITE_BUFFER, bytes, nullptr, GL_STATIC_DRAW);
        GLint64 actual = 0;
        gl_->glGetBufferParameteri64v(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &actual);
        if (actual != bytes) throw std::runtime_error("Cannot allocate polygon index buffer");
    }
    void append(const std::uint32_t* indices, std::size_t count) {
        if (count > capacity_ - written_) throw std::runtime_error("Polygon index count exceeds allocation");
        gl_->glBufferSubData(GL_COPY_WRITE_BUFFER, written_ * sizeof(std::uint32_t),
                             count * sizeof(std::uint32_t), indices);
        written_ += count;
        // Bound driver-side staging as well as the application's upload batch.
        // The next range is disjoint, but an asynchronous driver may retain
        // every submitted range until the first draw.
        if (written_ - lastDrain_ >= (8u << 20) / sizeof(std::uint32_t)) {
            gl_->glFinish();
            lastDrain_ = written_;
        }
    }
    void finish() {
        if (gl_->glGetError() != GL_NO_ERROR) throw std::runtime_error("Failed to upload polygon indices");
        target_->IndexCount = written_;
    }
private:
    QOpenGLExtraFunctions* gl_;
    vtkOpenGLIndexBufferObject* target_;
    std::size_t capacity_;
    std::size_t written_ = 0;
    std::size_t lastDrain_ = 0;
    GLint previous_ = 0;
};

template<typename T>
bool samePosition(const T* points, vtkIdType a, vtkIdType b) {
    const T* p = points + 3 * a;
    const T* q = points + 3 * b;
    return p[0] == q[0] && p[1] == q[1] && p[2] == q[2];
}

// VTK 8.2 fans each polygon and skips triangles with coincident corners.
// Preserve that order, including separate vertices with equal coordinates.
template<typename T>
void writeTriangles(vtkCellArray* polys, const T* points, IndexUpload& upload) {
    const std::size_t batchSize = 3 * 32768;
    std::unique_ptr<std::uint32_t[]> storage(new std::uint32_t[batchSize]);
    auto* batch = storage.get();
    std::size_t used = 0;
    const auto* face = polys->GetPointer();
    const auto* end = face + polys->GetNumberOfConnectivityEntries();
    while (face < end) {
        const auto width = *face++;
        const auto a = face[0];
        for (vtkIdType i = 2; i < width; ++i) {
            const auto b = face[i - 1], c = face[i];
            if (samePosition(points, a, b) || samePosition(points, a, c) || samePosition(points, b, c)) continue;
            batch[used++] = static_cast<std::uint32_t>(a);
            batch[used++] = static_cast<std::uint32_t>(b);
            batch[used++] = static_cast<std::uint32_t>(c);
            if (used == batchSize) { upload.append(batch, used); used = 0; }
        }
        face += width;
    }
    if (used) upload.append(batch, used);
}

}

vtkStandardNewMacro(BoundaryPolyDataMapper);

void BoundaryPolyDataMapper::SetSharedEdges(std::shared_ptr<const SharedPolygonEdges> edges) {
    if (edges_ == edges) return;
    edges_ = std::move(edges);
    inputTime_ = 0;
    Modified();
}

std::size_t BoundaryPolyDataMapper::GetSharedEdgeCount() const {
    if (!sharedBuffers_ || !showingEdges_) return 0;
    auto* buffer = Primitives[wireSlots_ ? PrimitiveTris : PrimitiveTrisEdges].IBO;
    return buffer->GetHandle() ? buffer->IndexCount / 2 : 0;
}

bool BoundaryPolyDataMapper::GetNeedToRebuildBufferObjects(vtkRenderer* renderer, vtkActor* actor) {
    const bool picking = renderer->GetSelector() || renderer->GetRenderWindow()->GetIsPicking();
    const bool changed = picking != picking_;
    picking_ = picking;
    return Superclass::GetNeedToRebuildBufferObjects(renderer, actor) || changed;
}

void BoundaryPolyDataMapper::UseWireSlots(bool wireframe) {
    if (wireframe == wireSlots_) return;
    // VTK draws wireframe polygons from PrimitiveTris, and overlay lines
    // from PrimitiveTrisEdges. Reuse the same uploaded lines in either slot.
    std::swap(Primitives[PrimitiveTris].IBO, Primitives[PrimitiveTrisEdges].IBO);
    wireSlots_ = wireframe;
}

void BoundaryPolyDataMapper::ClearIndexBuffers() {
    for (auto& primitive : Primitives) discard(primitive.IBO);
    inputTime_ = 0;
    showingEdges_ = false;
}

void BoundaryPolyDataMapper::BuildIBO(vtkRenderer* renderer, vtkActor* actor, vtkPolyData* poly) {
    auto* property = actor->GetProperty();
    const int mode = property->GetRepresentation();
    const bool wire = mode == VTK_WIREFRAME;
    const bool wantEdges = wire || property->GetEdgeVisibility();
    auto* context = QOpenGLContext::currentContext();
    auto* coords = poly->GetPoints() ? poly->GetPoints()->GetData() : nullptr;
    auto* doubles = vtkDoubleArray::SafeDownCast(coords);
    auto* floats = vtkFloatArray::SafeDownCast(coords);

    // Hardware selection and per-cell texture lookup depend on VTK's
    // primitive-to-cell map. The standard renderer handles those cases.
    bool supported = context && edges_ && !picking_ && !HaveCellNormals && !HaveCellScalars &&
        !HaveAppleBug && !property->GetVertexVisibility() && (wire || mode == VTK_SURFACE) &&
        (floats || doubles) && coords->GetNumberOfComponents() == 3 && edges_->matches(poly);
    const auto maxCount = static_cast<std::uint64_t>((std::numeric_limits<GLsizei>::max)());
    auto* polys = poly->GetPolys();
    std::uint64_t triangles = 0;
    if (supported) {
        triangles = static_cast<std::uint64_t>(polys->GetNumberOfConnectivityEntries()) -
                    3 * static_cast<std::uint64_t>(polys->GetNumberOfCells());
        supported = triangles <= maxCount / 3 && edges_->edgeCount() <= maxCount / 2;
    }
    if (!supported) {
        if (sharedBuffers_) {
            UseWireSlots(false);
            ClearIndexBuffers();
            IBOBuildState.Clear();
            sharedBuffers_ = false;
        }
        Superclass::BuildIBO(renderer, actor, poly);
        return;
    }

    if (!sharedBuffers_ || inputTime_ != poly->GetMTime()) {
        ClearIndexBuffers();
        sharedBuffers_ = true;
        inputTime_ = poly->GetMTime();
    }
    UseWireSlots(wire);
    auto* lines = Primitives[wire ? PrimitiveTris : PrimitiveTrisEdges].IBO;
    auto* surface = Primitives[wire ? PrimitiveTrisEdges : PrimitiveTris].IBO;
    showingEdges_ = false;
    if (wire && surface->GetHandle()) discard(surface);
    if (!wantEdges && lines->GetHandle()) discard(lines);

    // VTK has already uploaded these arrays. Its double-to-float conversion
    // vectors can otherwise retain one full CPU copy per attribute.
    for (const char* name : {"vertexMC", "normalMC"}) {
        auto* buffer = VBOs->GetVBO(name);
        if (buffer && buffer->GetPackedVBO().empty())
            std::vector<float>().swap(buffer->GetPackedVBO());
    }
    try {
        auto* gl = context->extraFunctions();
        if (!wire && !surface->GetHandle()) {
            IndexUpload upload(gl, surface, static_cast<std::size_t>(3 * triangles));
            upload.allocate();
            if (doubles) writeTriangles(polys, doubles->GetPointer(0), upload);
            else writeTriangles(polys, floats->GetPointer(0), upload);
            upload.finish();
        }
        if (wantEdges && !lines->GetHandle()) {
            IndexUpload upload(gl, lines, static_cast<std::size_t>(2 * edges_->edgeCount()));
            upload.allocate();
            edges_->forEachBatch(poly, [&](const std::uint32_t* data, std::size_t count) { upload.append(data, count); });
            upload.finish();
            if (lines->IndexCount != 2 * edges_->edgeCount()) throw std::runtime_error("Shared edge count mismatch");
        }
        showingEdges_ = wantEdges;
    } catch (const std::exception& e) {
        if (surface->GetHandle() && !surface->IndexCount) discard(surface);
        discard(lines);
        vtkErrorMacro(<< e.what());
    }
}

} }
