#pragma once

#include <vtkOpenGLPolyDataMapper.h>
#include <cstddef>
#include <memory>

namespace aerosim { namespace visualization {

class SharedPolygonEdges;

// The actor, attributes, and picking retain the original surface dataset.
// Only the OpenGL element buffers use the mesh's shared polygon edges.
class BoundaryPolyDataMapper : public vtkOpenGLPolyDataMapper {
public:
    static BoundaryPolyDataMapper* New();
    vtkTypeMacro(BoundaryPolyDataMapper, vtkOpenGLPolyDataMapper);
    void SetSharedEdges(std::shared_ptr<const SharedPolygonEdges> edges);
    std::size_t GetSharedEdgeCount() const;

protected:
    bool GetNeedToRebuildBufferObjects(vtkRenderer*, vtkActor*) override;
    void BuildIBO(vtkRenderer*, vtkActor*, vtkPolyData*) override;

private:
    BoundaryPolyDataMapper() = default;
    ~BoundaryPolyDataMapper() override = default;
    void UseWireSlots(bool wireframe);
    void ClearIndexBuffers();

    std::shared_ptr<const SharedPolygonEdges> edges_;
    vtkMTimeType inputTime_ = 0;
    bool sharedBuffers_ = false;
    bool wireSlots_ = false;
    bool picking_ = false;
    bool showingEdges_ = false;
};

} }
