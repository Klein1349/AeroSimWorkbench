#include "aerosim/visualization/scene/VtkSceneViewer.h"

#include <QVBoxLayout>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCubeSource.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace aerosim {
namespace visualization {

VtkSceneViewer::VtkSceneViewer(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("vtkSceneViewer"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    widget_ = new QVTKOpenGLNativeWidget(this);
    widget_->setObjectName(QStringLiteral("vtkRenderWidget"));
    layout->addWidget(widget_);

    renderWindow_ = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    renderer_ = vtkSmartPointer<vtkRenderer>::New();
    renderer_->SetBackground(0.055, 0.102, 0.157);
    renderWindow_->AddRenderer(renderer_);
    widget_->SetRenderWindow(renderWindow_);

    // This neutral primitive is only a Viewer readiness cue. It is not a
    // dataset or a replacement for the visualization data/scene service.
    vtkNew<vtkCubeSource> cube;
    cube->SetXLength(1.8);
    cube->SetYLength(0.8);
    cube->SetZLength(0.35);
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(cube->GetOutputPort());
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.25, 0.58, 0.85);
    actor->GetProperty()->SetOpacity(0.85);
    actor->GetProperty()->SetEdgeVisibility(true);
    actor->GetProperty()->SetEdgeColor(0.85, 0.92, 1.0);
    renderer_->AddActor(actor);
    renderer_->ResetCamera();
}

VtkSceneViewer::~VtkSceneViewer() = default;

void VtkSceneViewer::resetCamera()
{
    renderer_->ResetCamera();
    render();
}

void VtkSceneViewer::setCameraOrientation(const QString &orientation)
{
    vtkCamera *camera = renderer_->GetActiveCamera();
    camera->SetFocalPoint(0.0, 0.0, 0.0);
    if (orientation == QStringLiteral("front")) {
        camera->SetPosition(0.0, -4.0, 0.0);
        camera->SetViewUp(0.0, 0.0, 1.0);
    } else if (orientation == QStringLiteral("top")) {
        camera->SetPosition(0.0, 0.0, 4.0);
        camera->SetViewUp(0.0, 1.0, 0.0);
    } else if (orientation == QStringLiteral("side")) {
        camera->SetPosition(4.0, 0.0, 0.0);
        camera->SetViewUp(0.0, 0.0, 1.0);
    } else {
        resetCamera();
        return;
    }
    camera->OrthogonalizeViewUp();
    renderer_->ResetCameraClippingRange();
    render();
}

void VtkSceneViewer::render()
{
    renderWindow_->Render();
}

} // namespace visualization
} // namespace aerosim
