#include "MeshViewerBinding.h"

#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/MeshManagementPage.h"
#include "aerosim/visualization/data/BoundaryLoadService.h"
#include "aerosim/visualization/scene/VtkSceneViewer.h"

namespace aerosim {
using visualization::BoundaryLoadService;
using visualization::VtkSceneViewer;

void configureMeshViewerSurface()
{
    VtkSceneViewer::configureSurface();
}

bool installMeshViewer(MainWindow &window)
{
    auto *page = window.findChild<MeshManagementPage *>();
    if (!page) return false;

    auto *viewer = new VtkSceneViewer;
    if (!window.insertWidget(WidgetSlot::Viewport, viewer)) {
        delete viewer;
        return false;
    }

    auto *service = new BoundaryLoadService(viewer);
    QObject::connect(page, &MeshManagementPage::boundaryImportRequested,
                     service, &BoundaryLoadService::loadFiles);
    QObject::connect(page, &MeshManagementPage::boundaryRemoveRequested,
                     service, &BoundaryLoadService::remove);
    QObject::connect(page, &MeshManagementPage::boundaryRemoveRequested,
                     viewer, &VtkSceneViewer::removeObject);
    QObject::connect(page, &MeshManagementPage::boundaryVisibilityChanged,
                     viewer, &VtkSceneViewer::setObjectVisible);
    QObject::connect(page, &MeshManagementPage::boundaryAppearanceChanged,
                     viewer, &VtkSceneViewer::setObjectAppearance);
    QObject::connect(page, &MeshManagementPage::boundaryViewRequested,
                     viewer, &VtkSceneViewer::setCameraOrientation);
    QObject::connect(page, &MeshManagementPage::boundaryOrbitRequested,
                     viewer, &VtkSceneViewer::orbitCamera);
    QObject::connect(viewer, &VtkSceneViewer::boundaryPicked,
                     page, &MeshManagementPage::selectBoundary);
    QObject::connect(viewer, &VtkSceneViewer::cameraOrientationChanged,
                     page, &MeshManagementPage::setBoundaryCameraOrientation);
    QObject::connect(page, &MeshManagementPage::boundaryBackgroundColorsRequested,
                     viewer, &VtkSceneViewer::setBackground);
    QObject::connect(service, &BoundaryLoadService::started,
                     page, &MeshManagementPage::beginBoundary);
    QObject::connect(service, &BoundaryLoadService::loaded, page,
                     [page, viewer, service](const QString &id, const QString &, const QString &summary) {
        const auto mesh = service->mesh(id);
        if (!mesh) return;
        // Choose the large-mesh display mode before the actor's first render.
        const int initialMode = page->finishBoundary(id, summary, mesh->boundaryFaces);
        if (initialMode >= 0) viewer->addBoundary(id, mesh, initialMode);
    });
    QObject::connect(service, &BoundaryLoadService::batchImported, viewer, [viewer] {
        viewer->setCameraOrientation(QStringLiteral("fit"));
    });
    QObject::connect(service, &BoundaryLoadService::failed,
                     page, &MeshManagementPage::failBoundary);
    QObject::connect(service, &BoundaryLoadService::busyChanged,
                     page, &MeshManagementPage::setBoundaryBusy);

#ifdef AEROSIM_MESH_HAS_CGNS
    page->enableBoundaryMode(true);
#else
    page->enableBoundaryMode();
#endif
    viewer->setCameraOrientation(QStringLiteral("top"));
    return true;
}
} // namespace aerosim
