#pragma once

#include <QWidget>
#include <QString>

#include <vtkSmartPointer.h>

class QVTKOpenGLNativeWidget;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;

namespace aerosim {
namespace visualization {

// GUI-thread-only VTK viewport. It owns the render window and renderer, while
// future scene services own data and submit scene-object changes to this widget.
class VtkSceneViewer final : public QWidget {
    Q_OBJECT
public:
    explicit VtkSceneViewer(QWidget *parent = nullptr);
    ~VtkSceneViewer() override;

public slots:
    void resetCamera();
    void setCameraOrientation(const QString &orientation);

private:
    void render();

    QVTKOpenGLNativeWidget *widget_ = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow_;
    vtkSmartPointer<vtkRenderer> renderer_;
};

} // namespace visualization
} // namespace aerosim
