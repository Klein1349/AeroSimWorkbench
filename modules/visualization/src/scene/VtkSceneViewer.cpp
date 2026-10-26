#include "aerosim/visualization/scene/VtkSceneViewer.h"

#include <QVBoxLayout>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkCamera.h>
#include <vtkGenericOpenGLRenderWindow.h>
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
