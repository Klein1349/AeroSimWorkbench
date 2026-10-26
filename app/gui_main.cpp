#include "ApplicationController.h"
#include "MockTaskService.h"
#include "MockWorkflowService.h"
#include "aerosim/ui/MainWindow.h"

#include <QApplication>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    aerosim::ApplicationController controller(
        std::unique_ptr<aerosim::IWorkflowService>(new aerosim::MockWorkflowService),
        std::unique_ptr<aerosim::ITaskService>(new aerosim::MockTaskService));
    if (!controller.initialize()) {
        QTextStream(stderr) << "Failed to initialize application services\n";
        return 3;
    }

    aerosim::MainWindow window;
    window.bindServices({controller.workflow(), controller.tasks(), nullptr, nullptr, nullptr, nullptr});
    window.show();

    const int screenshotIndex = app.arguments().indexOf(QStringLiteral("--screenshot"));
    if (screenshotIndex >= 0) {
        if (screenshotIndex + 1 >= app.arguments().size()) return 2;
        const QString path = app.arguments().at(screenshotIndex + 1);
        QTimer::singleShot(500, &window, [&window, &app, path] {
            app.exit(window.grab().save(path) ? 0 : 2);
        });
    }
    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
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
