#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDockWidget>
#include <QLabel>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTextStream>

class Workflow final : public aerosim::IWorkflowService {
public:
    using IWorkflowService::IWorkflowService;
    aerosim::WorkflowView view(const aerosim::ScopeRef &scope) const override {
        aerosim::WorkflowView result;
        result.scope = scope;
        result.actions.append({QStringLiteral("run"), true, {}, {}, {}, {}, {}});
        return result;
    }
    aerosim::ValidationResult requestRun(const aerosim::RunRequest &) override { ++requests; return {true, {}}; }
    void onTaskEvent(const aerosim::TaskEvent &) override {}
    int requests = 0;
};

int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    aerosim::MainWindow window;
    const auto check = [](bool passed, const char *message) {
        if (!passed) QTextStream(stderr) << message << '\n';
        return passed;
    };
    if (!check(window.findChild<QTreeWidget *>("projectTree")->topLevelItemCount() == 5, "Missing project navigation groups")) return 1;
    auto *projectTree = window.findChild<QTreeWidget *>("projectTree");
    auto *equationsItem = projectTree->topLevelItem(1)->child(0);
    projectTree->setCurrentItem(equationsItem);
    auto *workspacePages = window.findChild<QStackedWidget *>("workspacePages");
    auto *parameterPage = window.findChild<QWidget *>("parameterSettingsPage");
    if (!check(workspacePages && parameterPage && !parameterPage->isWindow() &&
               workspacePages->currentWidget() == parameterPage && parameterPage->window() == &window &&
               workspacePages->widget(0)->isHidden() &&
               window.findChild<QDockWidget *>("propertyDock")->isHidden(),
               "Parameter settings must replace the viewport inside the main window")) return 17;
    auto *calculationType = window.findChild<QComboBox *>("calculationType");
    auto *euler = window.findChild<QRadioButton *>("eulerEquation");
    auto *navierStokes = window.findChild<QRadioButton *>("navierStokesEquation");
    auto *rarefied = window.findChild<QCheckBox *>("rarefiedGasEffect");
    if (!check(calculationType && euler && navierStokes && rarefied &&
               calculationType->count() == 2 &&
               calculationType->itemText(0) == QString::fromUtf16(u"定常计算") &&
               calculationType->itemText(1) == QString::fromUtf16(u"非定常计算") &&
               calculationType->currentData() == QStringLiteral("unsteady") && euler->isChecked() &&
               !navierStokes->isChecked() && !rarefied->isChecked() &&
               window.findChild<QRadioButton *>("inviscidFlow")->isChecked(),
               "Governing equation defaults do not match reference")) return 12;
    auto *boundaryItem = projectTree->topLevelItem(1)->child(1);
    projectTree->setCurrentItem(boundaryItem);
    auto *boundaryTable = window.findChild<QTreeWidget *>("boundaryTable");
    auto *boundaryName = window.findChild<QLineEdit *>("boundaryName");
    auto *boundaryType = window.findChild<QComboBox *>("boundaryType");
    auto *applyBoundary = window.findChild<QPushButton *>("applyBoundary");
    if (!check(boundaryTable && boundaryTable->topLevelItemCount() == 5 && boundaryName && boundaryType && applyBoundary &&
               boundaryTable->currentItem()->text(0) == QStringLiteral("wing_edge") &&
               boundaryTable->currentItem()->text(1) == QString::fromUtf16(u"无粘壁面") &&
               boundaryTable->currentItem()->text(2) == QStringLiteral("M6") &&
               boundaryType->currentData() == QStringLiteral("inviscid-wall"), "Boundary reference data is incomplete")) return 20;
    int boundaryDraftChanges = 0;
    QVariantMap boundaryDraft;
    QObject::connect(&window, &aerosim::MainWindow::parameterDraftChanged, &app,
                     [&](const QString &page, const QVariantMap &draft) {
        if (page == QStringLiteral("boundary")) { ++boundaryDraftChanges; boundaryDraft = draft; }
    });
    boundaryName->setText(QStringLiteral("wing_edge_updated"));
    boundaryType->setCurrentIndex(boundaryType->findData(QStringLiteral("farfield")));
    if (!check(applyBoundary->isEnabled(), "Boundary apply should enable after edits")) return 21;
    applyBoundary->click();
    if (!check(boundaryDraftChanges == 1 && boundaryDraft.value("selectedId") == QStringLiteral("wing_edge") &&
               boundaryTable->currentItem()->text(0) == QStringLiteral("wing_edge_updated") &&
               boundaryTable->currentItem()->text(1) == QString::fromUtf16(u"远场"), "Boundary apply did not update draft")) return 22;
    projectTree->setCurrentItem(equationsItem);
    projectTree->setCurrentItem(boundaryItem);
    if (!check(window.findChild<QLineEdit *>("boundaryName")->text() == QStringLiteral("wing_edge_updated") &&
               window.findChild<QComboBox *>("boundaryType")->currentData() == QStringLiteral("farfield"),
               "Boundary draft was not retained")) return 23;
    boundaryName = window.findChild<QLineEdit *>("boundaryName");
    applyBoundary = window.findChild<QPushButton *>("applyBoundary");
    boundaryName->setText(QString());
    if (!check(!applyBoundary->isEnabled(), "Empty boundary name should be rejected")) return 24;
    boundaryName->setText(QStringLiteral("symmetry"));
    if (!check(!applyBoundary->isEnabled(), "Duplicate boundary name should be rejected")) return 25;
    window.findChild<QPushButton *>("resetPropertyDraft")->click();
    if (!check(window.findChild<QLineEdit *>("boundaryName")->text() == QStringLiteral("wing_edge") &&
               window.findChild<QComboBox *>("boundaryType")->currentData() == QStringLiteral("inviscid-wall"),
               "Reset did not restore boundary reference data")) return 26;
    const QString boundaryCapture = qEnvironmentVariable("AEROSIM_BOUNDARY_CAPTURE_DIR");
    if (!boundaryCapture.isEmpty()) {
        QDir().mkpath(boundaryCapture);
        window.show();
        for (const QSize size : {QSize(1440, 960), QSize(1000, 720)}) {
            window.resize(size);
            app.processEvents();
            app.processEvents();
            if (!window.grab().save(boundaryCapture + QStringLiteral("/boundary-%1.png").arg(size.width()))) return 27;
        }
    }
    projectTree->setCurrentItem(equationsItem);
    calculationType = window.findChild<QComboBox *>("calculationType");
    euler = window.findChild<QRadioButton *>("eulerEquation");
    navierStokes = window.findChild<QRadioButton *>("navierStokesEquation");
    rarefied = window.findChild<QCheckBox *>("rarefiedGasEffect");
    int draftChanges = 0;
    QVariantMap equationDraft;
    QObject::connect(&window, &aerosim::MainWindow::parameterDraftChanged, &app,
                     [&](const QString &page, const QVariantMap &draft) {
        if (page == QStringLiteral("physics")) { ++draftChanges; equationDraft = draft; }
    });
    calculationType->setCurrentIndex(0);
    navierStokes->click();
    rarefied->click();
    if (!check(draftChanges == 3 && !euler->isChecked() && navierStokes->isChecked() &&
               equationDraft.value("calculationType") == QStringLiteral("steady") &&
               equationDraft.value("equation") == QStringLiteral("navier-stokes") &&
               equationDraft.value("flowRegime") == QStringLiteral("inviscid") &&
               equationDraft.value("rarefiedGas").toBool(), "Equation edits did not emit a complete draft")) return 13;
    projectTree->setCurrentItem(projectTree->topLevelItem(1)->child(1));
    projectTree->setCurrentItem(equationsItem);
    if (!check(window.findChild<QComboBox *>("calculationType")->currentIndex() == 0 &&
               window.findChild<QRadioButton *>("navierStokesEquation")->isChecked() &&
               window.findChild<QCheckBox *>("rarefiedGasEffect")->isChecked() && draftChanges == 3,
               "Navigation lost the equation draft or emitted duplicate changes")) return 14;
    window.findChild<QPushButton *>("resetPropertyDraft")->click();
    if (!check(window.findChild<QComboBox *>("calculationType")->currentIndex() == 1 &&
               window.findChild<QRadioButton *>("eulerEquation")->isChecked() &&
               !window.findChild<QCheckBox *>("rarefiedGasEffect")->isChecked(), "Equation draft reset failed")) return 15;
    const QString captureDir = qEnvironmentVariable("AEROSIM_EQUATIONS_CAPTURE_DIR");
    if (!captureDir.isEmpty()) {
        QDir().mkpath(captureDir);
        window.show();
        for (const QSize size : {QSize(1440, 960), QSize(1000, 720)}) {
            window.resize(size);
            app.processEvents();
            app.processEvents();
            if (!window.grab().save(captureDir + QStringLiteral("/equations-%1.png").arg(size.width()))) return 16;
        }
    }
    window.findChild<QPushButton *>("backToWorkbench")->click();
    if (!check(workspacePages->currentIndex() == 0, "Back button did not return to viewport")) return 18;
    projectTree->setCurrentItem(projectTree->topLevelItem(0)->child(0));
    projectTree->setCurrentItem(equationsItem);
    if (!check(workspacePages->currentWidget() == parameterPage, "Parameter page cannot be reopened")) return 19;
    auto *motionItem = projectTree->topLevelItem(1)->child(2);
    projectTree->setCurrentItem(motionItem);
    auto *motionEnabled = window.findChild<QCheckBox *>("meshMotionEnabled");
    auto *bodyCount = window.findChild<QSpinBox *>("motionBodyCount");
    auto *motionTable = window.findChild<QTableWidget *>("motionBodyTable");
    if (!check(motionEnabled && bodyCount && motionTable && !motionEnabled->isChecked() &&
               bodyCount->value() == 0 && motionTable->rowCount() == 0 && !bodyCount->isEnabled(),
               "Mesh motion defaults differ from the reference")) return 28;
    const QString motionCapture = qEnvironmentVariable("AEROSIM_MOTION_CAPTURE_DIR");
    if (!motionCapture.isEmpty()) {
        QDir().mkpath(motionCapture);
        window.show();
        window.resize(1000, 720);
        app.processEvents();
        app.processEvents();
        if (!window.grab().save(motionCapture + QStringLiteral("/motion-default.png"))) return 29;
    }
    QVariantMap motionDraft;
    QObject::connect(&window, &aerosim::MainWindow::parameterDraftChanged, &app,
                     [&](const QString &page, const QVariantMap &draft) {
        if (page == QStringLiteral("motion")) motionDraft = draft;
    });
    motionEnabled->click();
    bodyCount->setValue(2);
    if (!check(motionTable->rowCount() == 2 && bodyCount->isEnabled(), "Motion body count did not create rows")) return 30;
    window.findChild<QLineEdit *>("motionBody0_name")->setText(QStringLiteral("wing"));
    window.findChild<QLineEdit *>("motionBody0_target")->setText(QStringLiteral("M6"));
    window.findChild<QComboBox *>("motionBody0_type")->setCurrentIndex(1);
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    window.findChild<QDoubleSpinBox *>("motionComponent2")->setValue(2.5);
    if (!check(motionDraft.value("bodies").toList().at(0).toMap().value("angularVelocity").toList().at(2).toDouble() == 2.5,
               "Motion angular velocity did not update its draft")) return 31;
    bodyCount->setValue(0);
    bodyCount->setValue(2);
    if (!check(window.findChild<QLineEdit *>("motionBody0_name")->text() == QStringLiteral("wing"),
               "Reducing body count lost retained drafts")) return 32;
    projectTree->setCurrentItem(boundaryItem);
    projectTree->setCurrentItem(motionItem);
    if (!check(window.findChild<QSpinBox *>("motionBodyCount")->value() == 2 &&
               window.findChild<QCheckBox *>("meshMotionEnabled")->isChecked() &&
               window.findChild<QLineEdit *>("motionBody0_target")->text() == QStringLiteral("M6"),
               "Motion draft was not retained after navigation")) return 33;
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    if (!check(window.findChild<QDoubleSpinBox *>("motionComponent2")->value() == 2.5,
               "Motion parameter draft was lost after navigation")) return 34;
    if (!motionCapture.isEmpty()) {
        for (const QSize size : {QSize(1440, 960), QSize(1000, 720)}) {
            window.resize(size);
            app.processEvents();
            app.processEvents();
            if (!window.grab().save(motionCapture + QStringLiteral("/motion-%1.png").arg(size.width()))) return 35;
        }
    }
    window.findChild<QCheckBox *>("meshMotionEnabled")->click();
    if (!check(!window.findChild<QTableWidget *>("motionBodyTable")->isEnabled() &&
               window.findChild<QSpinBox *>("motionBodyCount")->value() == 2,
               "Disabling mesh motion should retain and disable its configuration")) return 36;
    window.findChild<QPushButton *>("resetPropertyDraft")->click();
    if (!check(window.findChild<QSpinBox *>("motionBodyCount")->value() == 0 &&
               !window.findChild<QCheckBox *>("meshMotionEnabled")->isChecked(), "Mesh motion reset failed")) return 37;
    window.hide();
    for (auto slot : {aerosim::WidgetSlot::Viewport, aerosim::WidgetSlot::Log, aerosim::WidgetSlot::Error, aerosim::WidgetSlot::Monitor}) {
        auto *widget = new QLabel("Injected component");
        if (!check(window.insertWidget(slot, widget) && window.isAncestorOf(widget), "Widget ownership not transferred")) return 2;
        auto *duplicate = new QLabel;
        bool rejected = !window.insertWidget(slot, duplicate) && !duplicate->parent();
        delete duplicate;
        if (!check(rejected, "Occupied slot must reject without taking ownership")) return 3;
    }
    aerosim::PostProcessWorkbenchWindow postprocess;
    QObject::connect(&window, &aerosim::MainWindow::openPostProcessRequested,
                     &postprocess, &QWidget::show);
    auto *openPostprocess = window.findChild<QAction *>("openPostProcessAction");
    if (!check(openPostprocess && openPostprocess->isEnabled(), "Missing postprocess entry")) return 9;
    openPostprocess->trigger();
    if (!check(postprocess.isVisible(), "Postprocess entry failed to open workbench")) return 10;
    postprocess.close();
    openPostprocess->trigger();
    if (!check(postprocess.isVisible(), "Closed workbench cannot be reopened")) return 11;
    postprocess.close();
    Workflow workflow;
    aerosim::ServiceBindings bindings;
    bindings.workflow = &workflow;
    window.bindServices(bindings);
    window.bindServices(bindings);
    aerosim::RunRequest request;
    request.scope.caseId = "case";
    request.inputSnapshot.snapshotId = "snapshot";
    window.setRunContext(request);
    auto *run = window.findChild<QAction *>("runAction");
    if (!check(run && run->isEnabled(), "Workflow action not reflected")) return 4;
    run->trigger();
    if (!check(workflow.requests == 1, "Repeated binding duplicated run requests")) return 5;
    auto view = workflow.view(request.scope);
    view.actions[0].enabled = false;
    window.displayWorkflow(view);
    if (!check(!run->isEnabled(), "Blocked action remains enabled")) return 6;
    aerosim::TaskEvent event;
    event.taskId = "test-task";
    event.status = aerosim::TaskStatus::Preparing;
    window.displayTaskEvent(event);
    event.status = aerosim::TaskStatus::Finished;
    window.displayTaskEvent(event);
    if (!check(window.findChild<QTreeWidget *>("taskTree")->topLevelItemCount() == 1, "Task rows not updated by id")) return 7;
    window.bindServices({});
    if (!check(!run->isEnabled(), "Unbinding must disable run")) return 8;
    QTextStream(stdout) << "UI ownership, service binding, workflow gating and task display passed\n";
    return 0;
}
