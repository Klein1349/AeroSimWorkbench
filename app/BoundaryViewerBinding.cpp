#include "BoundaryViewerBinding.h"
#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/MeshManagementPage.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include "aerosim/visualization/data/BoundaryLoadService.h"
#include "aerosim/visualization/scene/VtkSceneViewer.h"
namespace aerosim {
using visualization::BoundaryLoadService;
using visualization::VtkSceneViewer;
bool installBoundaryViewer(MainWindow& window) {
    auto* page=window.findChild<MeshManagementPage*>(); if(!page) return false;
    auto* viewer=new VtkSceneViewer;
    if(!window.insertWidget(WidgetSlot::Viewport,viewer)){delete viewer;return false;}
    auto* service=new BoundaryLoadService(viewer);
    QObject::connect(page,&MeshManagementPage::boundaryImportRequested,service,&BoundaryLoadService::load);
    QObject::connect(page,&MeshManagementPage::boundaryRemoveRequested,service,&BoundaryLoadService::remove);
    QObject::connect(page,&MeshManagementPage::boundaryRemoveRequested,viewer,&VtkSceneViewer::removeObject);
    QObject::connect(page,&MeshManagementPage::boundaryStyleChanged,viewer,&VtkSceneViewer::setObjectStyle);
    QObject::connect(page,&MeshManagementPage::boundaryViewRequested,viewer,&VtkSceneViewer::setCameraOrientation);
    QObject::connect(page,&MeshManagementPage::boundaryBackgroundRequested,viewer,[viewer](int mode){
        viewer->setBackground(mode==1?QColor("#273443"):QColor("#edf2f7"),QColor("#6f849b"),mode==2);
    });
    QObject::connect(service,&BoundaryLoadService::started,page,&MeshManagementPage::beginBoundary);
    QObject::connect(service,&BoundaryLoadService::loaded,page,[page,viewer,service](const QString& id,const QString&,const QString& summary){
        viewer->addBoundary(id,service->mesh(id)); page->finishBoundary(id,summary);
    });
    QObject::connect(service,&BoundaryLoadService::failed,page,&MeshManagementPage::failBoundary);
    QObject::connect(service,&BoundaryLoadService::busyChanged,page,&MeshManagementPage::setBoundaryBusy);
    page->enableBoundaryMode();return true;
}
bool installBoundaryViewer(PostProcessWorkbenchWindow& window) {
    auto* viewer=new VtkSceneViewer;
    if(!window.insertWidget(WidgetSlot::Viewport,viewer)){delete viewer;return false;}
    auto* service=new BoundaryLoadService(viewer);
    QObject::connect(&window,&PostProcessWorkbenchWindow::importFilesRequested,service,[service](const QStringList& paths){
        if(!paths.isEmpty()) service->load(paths.first());
    });
    QObject::connect(&window,&PostProcessWorkbenchWindow::boundaryRemoveRequested,service,&BoundaryLoadService::remove);
    QObject::connect(&window,&PostProcessWorkbenchWindow::boundaryRemoveRequested,viewer,&VtkSceneViewer::removeObject);
    QObject::connect(&window,&PostProcessWorkbenchWindow::boundaryStyleChanged,viewer,&VtkSceneViewer::setObjectStyle);
    QObject::connect(&window,&PostProcessWorkbenchWindow::viewRequested,viewer,&VtkSceneViewer::setCameraOrientation);
    QObject::connect(&window,&PostProcessWorkbenchWindow::backgroundChanged,viewer,&VtkSceneViewer::setBackground);
    QObject::connect(service,&BoundaryLoadService::started,&window,&PostProcessWorkbenchWindow::beginBoundary);
    QObject::connect(service,&BoundaryLoadService::loaded,&window,[&window,viewer,service](const QString& id,const QString& path,const QString& summary){
        viewer->addBoundary(id,service->mesh(id)); window.finishBoundary(id,path,summary);
    });
    QObject::connect(service,&BoundaryLoadService::failed,&window,&PostProcessWorkbenchWindow::failBoundary);
    QObject::connect(service,&BoundaryLoadService::busyChanged,&window,&PostProcessWorkbenchWindow::setBoundaryBusy);
    window.enableBoundaryMode();return true;
}
}
