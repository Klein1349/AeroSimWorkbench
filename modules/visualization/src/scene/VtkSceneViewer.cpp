#include "aerosim/visualization/scene/VtkSceneViewer.h"
#include <QVTKOpenGLNativeWidget.h>
#include <QVBoxLayout>
#include <QThread>
#include <QHash>
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <cmath>
namespace aerosim { namespace visualization {
struct VtkSceneViewer::Impl {
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> window=vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    vtkSmartPointer<vtkRenderer> renderer=vtkSmartPointer<vtkRenderer>::New();
    struct Object { vtkSmartPointer<vtkActor> actor; qint64 faces=0; };
    QHash<QString,Object> objects;
};
VtkSceneViewer::VtkSceneViewer(QWidget* parent):QWidget(parent),impl_(new Impl) {
    setObjectName(QStringLiteral("boundaryVtkViewer"));
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
    auto* widget=new QVTKOpenGLNativeWidget(this); layout->addWidget(widget);
    impl_->window->AddRenderer(impl_->renderer); widget->SetRenderWindow(impl_->window);
    vtkNew<vtkInteractorStyleTrackballCamera> style;
    impl_->window->GetInteractor()->SetInteractorStyle(style);
    impl_->renderer->SetBackground(0.15,0.19,0.25);
    impl_->renderer->SetBackground2(0.85,0.9,0.96); impl_->renderer->GradientBackgroundOn();
}
VtkSceneViewer::~VtkSceneViewer()=default;
void VtkSceneViewer::render() {
    Q_ASSERT(QThread::currentThread()==thread());
    if(isVisible()) impl_->window->Render();
}
void VtkSceneViewer::addBoundary(const QString& id,std::shared_ptr<const BoundaryMesh> mesh) {
    if(!mesh || impl_->objects.contains(id)) return;
    vtkNew<vtkPolyDataMapper> mapper; mapper->SetInputData(mesh->surface); mapper->ScalarVisibilityOff();
    Impl::Object obj; obj.actor=vtkSmartPointer<vtkActor>::New(); obj.actor->SetMapper(mapper);
    obj.actor->GetProperty()->SetColor(0.23,0.51,0.77);
    obj.actor->GetProperty()->SetEdgeColor(0.1,0.15,0.2); obj.actor->GetProperty()->EdgeVisibilityOn();
    obj.faces=mesh->boundaryFaces; impl_->renderer->AddActor(obj.actor); impl_->objects.insert(id,obj);
    if(impl_->objects.size()==1) resetCamera(); else render();
}
int VtkSceneViewer::objectCount() const {return impl_->objects.size();}
qint64 VtkSceneViewer::displayedFaces(const QString& id) const {return impl_->objects.value(id).faces;}
QVariantMap VtkSceneViewer::objectStyle(const QString& id) const {
    if(!impl_->objects.contains(id)) return {};
    auto* a=impl_->objects.value(id).actor.GetPointer();auto* p=a->GetProperty();
    return {{"visible",bool(a->GetVisibility())},{"opacity",p->GetOpacity()*100.0},
        {"mode",p->GetRepresentation()==VTK_WIREFRAME?1:(p->GetEdgeVisibility()?2:0)},
        {"scale",a->GetScale()[0]}};
}
void VtkSceneViewer::removeObject(const QString& id) {
    if(!impl_->objects.contains(id)) return;
    impl_->renderer->RemoveActor(impl_->objects.take(id).actor); render();
}
void VtkSceneViewer::setObjectStyle(const QString& id,const QVariantMap& s) {
    if(!impl_->objects.contains(id)) return;
    auto* a=impl_->objects[id].actor.GetPointer(); auto* p=a->GetProperty();
    if(s.contains("visible")) a->SetVisibility(s.value("visible").toBool());
    if(s.contains("color")) { QColor c=s.value("color").value<QColor>(); if(c.isValid()) p->SetColor(c.redF(),c.greenF(),c.blueF()); }
    if(s.contains("opacity")) p->SetOpacity(qBound(0.0,s.value("opacity").toDouble()/100.0,1.0));
    if(s.contains("scale")) { double v=s.value("scale").toDouble(); if(std::isfinite(v)&&v>0) a->SetScale(v); }
    if(s.contains("mode")) {
        const int m=s.value("mode").toInt();
        p->SetRepresentation(m==1?VTK_WIREFRAME:VTK_SURFACE); p->SetEdgeVisibility(m==2);
    }
    impl_->renderer->ResetCameraClippingRange(); render();
}
void VtkSceneViewer::setBackground(const QColor& b,const QColor& t,bool g) {
    impl_->renderer->SetBackground(b.redF(),b.greenF(),b.blueF());
    impl_->renderer->SetBackground2(t.redF(),t.greenF(),t.blueF()); impl_->renderer->SetGradientBackground(g); render();
}
void VtkSceneViewer::resetCamera() { setCameraOrientation(QStringLiteral("reset")); }
void VtkSceneViewer::setCameraOrientation(const QString& command) {
    if(impl_->objects.isEmpty()) {render();return;}
    double bounds[6]; impl_->renderer->ComputeVisiblePropBounds(bounds);
    if(bounds[0]>bounds[1] || !std::isfinite(bounds[0])) return;
    auto* camera=impl_->renderer->GetActiveCamera();
    if(command=="in" || command=="out") camera->Dolly(command=="in"?1.2:1.0/1.2);
    else if(command!="fit") {
        double c[3]={(bounds[0]+bounds[1])/2,(bounds[2]+bounds[3])/2,(bounds[4]+bounds[5])/2};
        double d=qMax(1.0,qMax(bounds[1]-bounds[0],qMax(bounds[3]-bounds[2],bounds[5]-bounds[4])));
        camera->SetFocalPoint(c); camera->SetViewUp(0,0,1);
        if(command=="front") camera->SetPosition(c[0],c[1]-d,c[2]);
        else if(command=="right" || command=="side") camera->SetPosition(c[0]+d,c[1],c[2]);
        else if(command=="top") {camera->SetPosition(c[0],c[1],c[2]+d);camera->SetViewUp(0,1,0);}
        else camera->SetPosition(c[0]+d,c[1]-d,c[2]+d);
    }
    if(command!="in" && command!="out") {
        impl_->renderer->ResetCamera(bounds);
        camera->Zoom(0.9); // Leave a margin when the navigation panel opens.
    }
    impl_->renderer->ResetCameraClippingRange(); render();
}
} }
