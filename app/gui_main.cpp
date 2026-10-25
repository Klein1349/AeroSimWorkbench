#include "ApplicationController.h"
#include "MockTaskService.h"
#include "MockWorkflowService.h"
#include "aerosim/ui/PostProcessWorkbenchWindow.h"

#ifdef AEROSIM_HAS_VTK_VIEWER
#include "aerosim/visualization/scene/VtkSceneViewer.h"
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#endif

#include <QApplication>
#include <QTextStream>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
#ifdef AEROSIM_HAS_VTK_VIEWER
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
#endif
    QApplication app(argc, argv);
    aerosim::ApplicationController controller(
        std::unique_ptr<aerosim::IWorkflowService>(new aerosim::MockWorkflowService),
        std::unique_ptr<aerosim::ITaskService>(new aerosim::MockTaskService));
    if (!controller.initialize()) {
        QTextStream(stderr) << "Failed to initialize application services\n";
        return 3;
    }

    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
    aerosim::PostProcessWorkbenchWindow window;
    window.bindServices({controller.workflow(), controller.tasks(), nullptr, nullptr, nullptr, nullptr});
#ifdef AEROSIM_HAS_VTK_VIEWER
    if (!smoke) {
        auto *viewer = new aerosim::visualization::VtkSceneViewer;
        if (!window.insertWidget(aerosim::WidgetSlot::Viewport, viewer)) {
            delete viewer;
            QTextStream(stderr) << "Failed to insert VTK viewer\n";
            return 4;
        }
        QObject::connect(&window, &aerosim::PostProcessWorkbenchWindow::viewRequested, viewer,
                         [viewer](const QString &orientation) {
            if (orientation == QStringLiteral("reset"))
                viewer->resetCamera();
            else
                viewer->setCameraOrientation(orientation);
        });
    }
#endif
    window.show();

    bool smokePassed = false;
    if (smoke) {
        QObject::connect(controller.tasks(), &aerosim::ITaskService::taskEventOccurred, &app,
                         [&app, &smokePassed](const aerosim::TaskEvent &event) {
            if (event.type == aerosim::TaskEventType::Finished) {
                smokePassed = true;
                QTextStream(stdout) << "MOCK GUI smoke passed\n";
                app.quit();
            }
        }, Qt::QueuedConnection);
        QTimer::singleShot(0, &app, [&controller] {
            aerosim::RunRequest request;
            request.scope.caseId = QStringLiteral("mock-case");
            request.inputSnapshot.snapshotId = QStringLiteral("mock-snapshot");
            controller.workflow()->requestRun(request);
        });
        QTimer::singleShot(3000, &app, &QCoreApplication::quit);
    }
    const int code = app.exec();
    controller.shutdown();
    return smoke && !smokePassed ? 7 : code;
}
