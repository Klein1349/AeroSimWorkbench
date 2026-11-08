#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/ForcedMotionDialog.h"
#include "aerosim/ui/FixedAxisMotionDialog.h"
#include "aerosim/ui/SixDofMotionDialog.h"
#include "aerosim/ui/MeshMotionPanel.h"
#include <QDialogButtonBox>
#include "aerosim/ui/GoverningEquationsPanel.h"
#include "aerosim/ui/ReferenceValuesPanel.h"
#include "aerosim/ui/CalculationControlPanel.h"
#include "aerosim/ui/SolutionMethodPanel.h"
#include "aerosim/ui/OutputControlPanel.h"
#include "aerosim/ui/InitializationPanel.h"
#include "aerosim/ui/BoundaryConditionsPanel.h"
#include "aerosim/ui/FarfieldParametersPanel.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include <QAbstractButton>
#include <QAction>
#include <QSettings>
#include <QTemporaryDir>
#include <QDir>
#include <QPixmap>
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QDockWidget>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>

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
    QApplication app(argc, argv);
    QTemporaryDir preferences;
    if (!preferences.isValid()) return 22;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    aerosim::MainWindow window;
    const auto check = [](bool passed, const char *message) {
        if (!passed) QTextStream(stderr) << message << '\n';
        return passed;
    };
    // ASCII source with explicit UTF-16 expectations detects compiler/code-page corruption.
    const auto text = [](const char16_t *value) {
        return QString::fromUtf16(reinterpret_cast<const ushort *>(value));
    };
    if (!check(window.windowTitle() == text(u"AeroSimWorkbench \u00b7 \u4eff\u771f\u5de5\u4f5c\u53f0\uff08Mock \u670d\u52a1\uff09"),
               "Window title has incorrect Unicode characters")) return 12;
    if (!check(window.findChild<QDockWidget *>("projectDock")->windowTitle() == text(u"\u5de5\u4f5c\u533a\u6811"),
               "Dock title has incorrect Unicode characters")) return 13;
    if (!check(window.findChild<QTreeWidget *>("workspaceTree")->topLevelItem(0)->child(0)->text(0)
               == text(u"\u524d\u5904\u7406"), "Workspace tree label has incorrect Unicode characters")) return 14;
    bool hasNewProject = false;
    for (auto *action : window.findChildren<QAction *>())
        hasNewProject = hasNewProject || action->text() == text(u"\u65b0\u5efa\u5de5\u4f5c\u533a");
    if (!check(hasNewProject, "Workspace Ribbon action has incorrect Unicode characters")) return 15;
    int startupRequests = 0;
    QObject::connect(&window, &aerosim::MainWindow::showStartupRequested,
                     &window, [&startupRequests] { ++startupRequests; });
    auto *returnToStartup = window.findChild<QAbstractButton *>("returnToStartupButton");
    if (!check(returnToStartup
               && returnToStartup == window.ribbonBar()->applicationButton()
               && returnToStartup->text() == text(u"\u8fd4\u56de\u6b22\u8fce\u9875"),
               "Ribbon application button must return to the startup page")) return 69;
    returnToStartup->click();
    if (!check(startupRequests == 1, "Startup button did not emit the existing navigation request")) return 70;
    if (!check(window.findChild<QLabel *>("propertyTitle")->text() == text(u"\u7f51\u683c\u6587\u4ef6"),
               "Property title has incorrect Unicode characters")) return 16;
    if (!check(window.findChild<QTreeWidget *>("workspaceTree")->topLevelItemCount() == 1, "Missing workspace navigation")) return 1;
    const auto categories = window.ribbonBar()->categoryPages();
    const QStringList expectedCategories = {QStringLiteral("preprocessCategory"), QStringLiteral("parametersCategory"),
        QStringLiteral("calculationCategory"), QStringLiteral("intelligentDataCategory"),
        QStringLiteral("postprocessAnalysisCategory"), QStringLiteral("helpCategory")};
    bool categoriesMatch = categories.size() == expectedCategories.size();
    for (int index = 0; categoriesMatch && index < categories.size(); ++index)
        categoriesMatch = categories[index]->objectName() == expectedCategories[index];
    if (!check(categoriesMatch, "Expected five functional Ribbon tabs followed by Help")) return 17;
    auto *preprocess = categories[0];
    auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
    auto *projectTree = window.findChild<QTreeWidget *>("workspaceTree");
    auto *root = projectTree ? projectTree->topLevelItem(0) : nullptr;
    if (!check(workspace && projectTree && root && root->childCount() == 5
               && !window.findChild<QStackedWidget *>("navigationStack")
               && !window.findChild<QTreeWidget *>("parameterTree"),
               "Workspace navigation must use one unified tree")) return 32;
    auto *preprocessNode = root->child(0);
    auto *parameterNode = root->child(1);
    auto *calculationNode = root->child(2);
    auto *intelligentDataNode = root->child(3);
    auto *postprocessNode = root->child(4);
    if (!check(preprocessNode->childCount() == 3 && parameterNode->childCount() == 8
               && calculationNode->childCount() == 2
               && intelligentDataNode->childCount() == 2 && postprocessNode->childCount() == 0,
               "Unified tree module hierarchy is incorrect")) return 54;
    auto *helpCategory = categories[5];
    auto *pageBeforeHelp = workspace->currentWidget();
    auto *treeItemBeforeHelp = projectTree->currentItem();
    window.ribbonBar()->raiseCategory(helpCategory);
    app.processEvents();
    if (!check(workspace->currentWidget() == pageBeforeHelp
               && projectTree->currentItem() == treeItemBeforeHelp
               && root->childCount() == 5,
               "Help Ribbon must not alter the business workspace or unified tree")) return 80;
    window.ribbonBar()->raiseCategory(preprocess);
    auto *meshPage = window.findChild<QWidget *>("viewportPage");
    auto *meshList = window.findChild<QTreeWidget *>("meshList");
    auto *meshCount = window.findChild<QLabel *>("meshObjectCount");
    auto *addMesh = window.findChild<QPushButton *>("addMeshButton");
    auto *removeMesh = window.findChild<QPushButton *>("removeMeshButton");
    auto *scaleEditor = window.findChild<QDoubleSpinBox *>("meshScaleEditor");
    auto *historyTabs = window.findChild<QTabWidget *>("meshHistoryTabs");
    auto *history = window.findChild<QListWidget *>("meshOperationHistory");
    auto *historyCollapse = window.findChild<QPushButton *>("meshHistoryCollapseButton");
    if (!check(meshPage && meshList && meshList->topLevelItemCount() == 3
               && meshCount && meshCount->text().contains("3 / 10")
               && addMesh && removeMesh && scaleEditor && historyTabs && history && historyCollapse
               && window.findChild<QSplitter *>("meshWorkspaceMainSplitter")
               && window.findChild<QWidget *>("viewportSlot")
               && window.findChild<QDockWidget *>("propertyDock")->isHidden()
               && window.findChild<QWidget *>("outputBody")->isHidden()
               && historyTabs->isHidden()
               && historyCollapse->text() == text(u"\u5c55\u5f00")
               && !window.findChild<QPlainTextEdit *>("meshScriptPlaceholder")->isEnabled(),
               "Mesh management workspace is incomplete")) return 57;
    meshList->setCurrentItem(meshList->topLevelItem(1));
    scaleEditor->setValue(2.5);
    meshList->topLevelItem(1)->setCheckState(0, Qt::Unchecked);
    meshList->setCurrentItem(meshList->topLevelItem(0));
    meshList->setCurrentItem(meshList->topLevelItem(1));
    app.processEvents();
    if (!check(scaleEditor->value() == 2.5
               && !window.findChild<QCheckBox *>("meshVisibleEditor")->isChecked()
               && history->count() >= 3,
               "Per-mesh mock display state was not retained")) return 58;
    for (int i = meshList->topLevelItemCount(); i < 10; ++i) addMesh->click();
    if (!check(meshList->topLevelItemCount() == 10 && !addMesh->isEnabled()
               && meshCount->text().contains("10 / 10"),
               "Mesh management did not enforce its ten-object UI limit")) return 59;
    removeMesh->click();
    if (!check(meshList->topLevelItemCount() == 9 && addMesh->isEnabled(),
               "Removing a mock mesh did not update list availability")) return 60;
    historyCollapse->click();
    app.processEvents();
    if (!check(!historyTabs->isHidden() && historyCollapse->text() == text(u"\u6536\u8d77"),
               "Mesh operation history did not expand")) return 61;
    historyCollapse->click();
    app.processEvents();
    if (!check(historyTabs->isHidden(), "Mesh operation history did not collapse")) return 62;
    if (!check(window.findChild<QTreeWidget *>("workflowTree")->isHidden(),
               "Workflow tree must not occupy the module navigation")) return 33;
    projectTree->setCurrentItem(preprocessNode->child(0)->child(2));
    projectTree->setCurrentItem(preprocessNode->child(0)->child(1));
    app.processEvents();
    auto *retainedPage = workspace->currentWidget();
    if (!check(retainedPage->objectName() == "mesh-transformPage",
               "Preprocess node did not establish its module page state")) return 66;
    parameterNode->setExpanded(false);
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("parametersCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == parameterNode && parameterNode->isExpanded()
               && workspace->currentWidget()->objectName() == "parameterWorkspacePage"
               && workspace->currentWidget() != retainedPage
               && !workspace->currentWidget()->findChild<QWidget *>("viewportSlot"),
               "Parameter Ribbon did not switch the central module page")) return 34;
    if (!check(parameterNode->childCount() == 8 && parameterNode->child(0)->childCount() == 0,
               "Parameter tree must contain eight leaf pages")) return 41;
    projectTree->setCurrentItem(parameterNode->child(0));
    app.processEvents();
    auto *parameterDialog = window.findChild<QWidget *>("parameterEditor_control-equations");
    if (!check(parameterDialog && !parameterDialog->isHidden()
               && workspace->currentWidget()->objectName() == "parameterEditor_control-equations"
               && !parameterDialog->isWindow()
               && window.findChild<QRadioButton *>("calculationType_steady")
               && window.findChild<QLabel *>("parameterWorkspaceTitle")->text().contains(
                    text(u"\u63a7\u5236\u65b9\u7a0b"))
               && window.ribbonBar()->currentIndex() == window.ribbonBar()->categoryIndex(
                   window.findChild<SARibbonCategory *>("parametersCategory")),
               "Parameter node did not embed its editor or reverse-sync Ribbon")) return 52;

    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("calculationCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == calculationNode && calculationNode->isExpanded()
               && parameterNode->isExpanded()
               && workspace->currentWidget()->objectName() == "calculationOverviewPage",
               "Calculation Ribbon did not locate its tree node and overview")) return 55;
    auto *solverOutputMode = window.findChild<QComboBox *>("solverOutputModeCombo");
    auto *solverOutputDescription = window.findChild<QLabel *>("solverOutputModeDescription");
    if (!check(solverOutputMode && solverOutputMode->count() == 2
               && solverOutputMode->currentData().toString() == "embedded",
               "Calculation preparation is missing its default embedded output mode")) return 71;
    solverOutputMode->setCurrentIndex(1);
    if (!check(solverOutputDescription->text().contains(text(u"\u72ec\u7acb\u63a7\u5236\u53f0")),
               "Solver output mode did not retain its mock console configuration")) return 72;
    auto *rememberedMonitorItem = calculationNode->child(1)->child(0)->child(0);
    projectTree->setCurrentItem(rememberedMonitorItem);
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("intelligentDataCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == intelligentDataNode
               && workspace->currentWidget()->objectName() == "intelligentDataPage",
               "Intelligent data Ribbon did not show its reserved page")) return 53;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("calculationCategory"));
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "taskMonitorPage",
               "Calculation module did not restore its last monitor page")) return 67;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("postprocessAnalysisCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == postprocessNode
               && workspace->currentWidget()->objectName() == "postprocessOverviewPage",
               "Postprocess Ribbon did not locate its node and reserved page")) return 36;
    window.ribbonBar()->raiseCategory(preprocess);
    app.processEvents();
    if (!check(projectTree->currentItem() == preprocessNode
               && workspace->currentWidget() == retainedPage
               && workspace->currentWidget()->objectName() == "mesh-transformPage",
               "Preprocess Ribbon did not restore its last page")) return 51;
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (auto *category : categories) {
            window.ribbonBar()->raiseCategory(category);
            app.processEvents();
        }
    }
    window.ribbonBar()->raiseCategory(preprocess);
    app.processEvents();
    if (!check(workspace->currentWidget() == retainedPage
               && window.findChildren<QWidget *>("viewportSlot").size() == 1
               && meshList->topLevelItemCount() == 9,
               "Repeated module switching recreated state or the VTK host")) return 68;
    window.show();
    app.processEvents();
    auto *projectDock = window.findChild<QDockWidget *>("projectDock");
    auto *propertyDock = window.findChild<QDockWidget *>("propertyDock");
    if (!check(window.dockWidgetArea(projectDock) == Qt::LeftDockWidgetArea
               && window.dockWidgetArea(propertyDock) == Qt::LeftDockWidgetArea
               && projectDock->geometry().left() < propertyDock->geometry().left()
               && projectDock->minimumWidth() >= 220 && propertyDock->minimumWidth() >= 300,
               "Default layout must place navigation before properties on the left")) return 42;
    auto *outputDock = window.findChild<QDockWidget *>("outputDock");
    auto *outputTable = window.findChild<QTableWidget *>("outputLogTable");
    auto *levelFilter = window.findChild<QComboBox *>("logLevelFilter");
    auto *moduleFilter = window.findChild<QComboBox *>("logModuleFilter");
    auto *logSearch = window.findChild<QLineEdit *>("logSearch");
    if (!check(outputDock && outputDock->windowTitle() == text(u"\u8f93\u51fa")
               && outputTable && outputTable->columnCount() == 4
               && !outputDock->findChild<QTabWidget *>()
               && levelFilter && moduleFilter && logSearch
               && window.findChild<QCheckBox *>("logAutoScroll")->isChecked()
               && !window.findChild<QPushButton *>("exportOutputButton")->isEnabled(),
               "Bottom area must be a single structured Output panel")) return 44;
    auto *collapseOutput = window.findChild<QPushButton *>("outputCollapseButton");
    if (!check(window.findChild<QWidget *>("outputBody")->isHidden()
               && collapseOutput->text() == text(u"\u5c55\u5f00"),
               "Mesh workspace must initially use compact global output")) return 64;
    collapseOutput->click();
    app.processEvents();
    window.findChild<QPushButton *>("outputScopeReset")->click();
    levelFilter->setCurrentIndex(levelFilter->findData("WARN"));
    moduleFilter->setCurrentIndex(moduleFilter->findData("Validation"));
    logSearch->setText(text(u"\u5de5\u51b5"));
    app.processEvents();
    int visibleLogs = 0;
    for (int row = 0; row < outputTable->rowCount(); ++row) visibleLogs += !outputTable->isRowHidden(row);
    if (!check(visibleLogs == 1, "Output filters and search did not combine correctly")) return 45;
    levelFilter->setCurrentIndex(levelFilter->findData("ALL"));
    moduleFilter->setCurrentIndex(moduleFilter->findData("ALL"));
    logSearch->clear();
    collapseOutput->click();
    app.processEvents();
    if (!check(window.findChild<QWidget *>("outputBody")->isHidden()
               && collapseOutput->text() == text(u"\u5c55\u5f00"),
               "Output panel did not collapse to its toolbar")) return 46;
    collapseOutput->click();
    app.processEvents();
    if (!check(!window.findChild<QWidget *>("outputBody")->isHidden(),
               "Output panel did not restore its content")) return 47;
    if (!check(preprocess->panelByObjectName("workspaceGroup")
               && preprocess->panelByObjectName("meshManagementGroup")
               && preprocess->panelByObjectName("trajectoryGroup")
               && preprocess->panelByObjectName("viewGroup"), "Missing grouped Preprocess commands")) return 18;
    if (!check(window.findChild<QAction *>("showStartupAction")
               && window.findChild<QAction *>("showStartupAction")->isEnabled(),
               "Workbench must expose a return-to-startup entry")) return 56;
    auto *postprocessCategory = window.findChild<SARibbonCategory *>("postprocessAnalysisCategory");
    auto *helpRibbonCategory = window.findChild<SARibbonCategory *>("helpCategory");
    if (!check(postprocessCategory && !postprocessCategory->panelByObjectName("toolsGroup")
               && helpRibbonCategory && helpRibbonCategory->panelByObjectName("appearanceGroup")
               && helpRibbonCategory->panelByObjectName("layoutToolsGroup")
               && helpRibbonCategory->panelByObjectName("helpInfoGroup"),
               "Global appearance, layout and help tools must be centralized under Help")) return 19;
    auto *newProjectMenu = window.findChild<QMenu *>("newProjectMenu");
    auto *displayModeMenu = window.findChild<QMenu *>("displayModeMenu");
    if (!check(newProjectMenu && newProjectMenu->actions().size() == 3
               && displayModeMenu && displayModeMenu->actions().size() == 2,
               "Missing Word-style Ribbon drop-down menus")) return 30;
    if (!check(!window.findChild<QAction *>("blankProjectMenuAction")->isEnabled()
               && !window.findChild<QAction *>("surfaceModeMenuAction")->isEnabled(),
               "Drop-down menu availability does not match service boundaries")) return 31;
    auto *plansItem = calculationNode->child(0)->child(1);
    projectTree->setCurrentItem(plansItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "batchPlanPage"
               && projectTree->currentItem()->data(0, Qt::UserRole + 1).toString() == "batch-plans",
               "Batch configuration did not locate and open BatchPlan management")) return 38;
    if (!check(window.findChild<QTableWidget *>("batchPlanTable")->columnCount() == 3,
               "BatchPlan page is missing its model-neutral table")) return 39;
    auto *batchRunItem = calculationNode->child(1)->child(1)->child(0);
    auto *batchTaskItem = batchRunItem->child(0);
    projectTree->setCurrentItem(batchRunItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "batchMonitorPage"
               && window.findChild<QLabel *>("outputTitle")->text().contains("BatchRun_001")
               && window.findChild<QTreeWidget *>("taskTree")->topLevelItemCount() == 3,
               "BatchRun selection did not open the integrated batch monitor")) return 48;
    projectTree->setCurrentItem(batchTaskItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "taskMonitorPage"
               && window.findChild<QLabel *>("taskMonitorTitle")->text().contains("Task_001")
               && window.findChild<QWidget *>("convergenceCurvePanel")
               && window.findChild<QLabel *>("taskResourceSummary")
               && window.findChild<QLabel *>("outputTitle")->text().contains("Task_001"),
               "Task selection did not open the integrated monitor and scoped output")) return 49;
    auto *taskLogSplitter = window.findChild<QSplitter *>("taskMonitorContentSplitter");
    auto *taskLogView = window.findChild<QPlainTextEdit *>("taskRuntimeLogView");
    auto *taskLogFilter = window.findChild<QComboBox *>("taskLogStreamFilter");
    auto *appendTaskLog = window.findChild<QPushButton *>("appendTaskLogMockButton");
    auto *updateTaskProgress = window.findChild<QPushButton *>("updateTaskProgressMockButton");
    if (!check(taskLogSplitter && taskLogSplitter->count() == 2
               && taskLogSplitter->widget(0)->objectName() == "convergenceCurvePanel"
               && taskLogSplitter->widget(1)->objectName() == "taskRuntimeLogPanel"
               && taskLogView && taskLogView->isReadOnly()
               && taskLogView->lineWrapMode() == QPlainTextEdit::NoWrap
               && taskLogView->toPlainText().contains("Task_001")
               && taskLogFilter && taskLogFilter->count() == 3,
               "Task monitor must place a selectable fixed-font runtime log below its curve")) return 73;
    taskLogFilter->setCurrentIndex(taskLogFilter->findData("stderr"));
    app.processEvents();
    if (!check(taskLogView->toPlainText().contains("Warning")
               && !taskLogView->toPlainText().contains("Solver started"),
               "stderr filter changed or mixed the task log source data")) return 74;
    taskLogFilter->setCurrentIndex(taskLogFilter->findData("all"));
    app.processEvents();
    const int beforeAppendBlocks = taskLogView->document()->blockCount();
    appendTaskLog->click();
    app.processEvents();
    if (!check(taskLogView->document()->blockCount() == beforeAppendBlocks + 1
               && taskLogView->toPlainText().contains(text(u"Mock \u8ffd\u52a0")),
               "Line-oriented mock output was not appended incrementally")) return 75;
    const int beforeProgressBlocks = taskLogView->document()->blockCount();
    updateTaskProgress->click();
    app.processEvents();
    if (!check(taskLogView->document()->blockCount() == beforeProgressBlocks
               && taskLogView->toPlainText().contains("same-line update mock"),
               "Progress mock did not update the existing line in place")) return 76;
    projectTree->setCurrentItem(batchRunItem->child(1));
    app.processEvents();
    if (!check(window.findChild<QLabel *>("taskMonitorTitle")->text().contains("Task_002")
               && taskLogView->toPlainText().contains("Task_002")
               && !taskLogView->toPlainText().contains("Task_001"),
               "Switching batch tasks left the previous task log visible")) return 77;
    projectTree->setCurrentItem(batchRunItem->child(2));
    app.processEvents();
    if (!check(window.findChild<QLabel *>("taskMonitorTitle")->text().contains("Task_003")
               && taskLogView->toPlainText().isEmpty()
               && !taskLogView->placeholderText().isEmpty(),
               "Task without logs did not show the task-specific empty state")) return 78;
    projectTree->setCurrentItem(batchTaskItem);
    app.processEvents();
    if (!check(taskLogView->toPlainText().contains(text(u"Mock \u8ffd\u52a0")),
               "Returning to a task did not preserve its mock log state")) return 79;
    for (auto *action : window.findChildren<QAction *>()) {
        if (action->property("ribbonGroup").isValid()
            && !check(!action->icon().pixmap(32, 32).isNull(), "Missing embedded Ribbon icon")) return 20;
    }
    projectTree->setCurrentItem(preprocessNode->child(0)->child(2));
    projectTree->setCurrentItem(preprocessNode->child(0)->child(1));
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "mesh-transformPage",
               "Mesh transform node did not open its placeholder page")) return 43;
    if (!check(!propertyDock->isHidden(),
               "Non-mesh workspace did not restore the shared property panel")) return 65;
    projectTree->setCurrentItem(preprocessNode->child(0));
    app.processEvents();
    if (!check(workspace->currentWidget() == meshPage && meshList->topLevelItemCount() == 9
               && propertyDock->isHidden()
               && !window.findChild<QWidget *>("outputBody")->isHidden()
               && projectDock->width() <= 320
               && window.findChild<QWidget *>("meshViewerPanel")->width()
                    > window.findChild<QWidget *>("meshObjectPanel")->width(),
               "Mesh management group did not restore its persistent workspace")) return 63;
    app.processEvents();
    auto *saveUserLayout = window.findChild<QAction *>("saveUserLayoutAction");
    auto *restoreUserLayout = window.findChild<QAction *>("restoreUserLayoutAction");
    auto *restoreDefaultLayout = window.findChild<QAction *>("layoutAction");
    auto *meshMainSplitter = window.findChild<QSplitter *>("meshWorkspaceMainSplitter");
    auto *monitorLayoutSplitter = window.findChild<QSplitter *>("taskMonitorContentSplitter");
    if (!check(saveUserLayout && restoreUserLayout
               && restoreDefaultLayout && meshMainSplitter && monitorLayoutSplitter,
               "Help layout actions or adjustable page splitters are missing")) return 81;
    meshMainSplitter->setSizes({390, 610});
    monitorLayoutSplitter->setSizes({720, 280});
    saveUserLayout->trigger();
    app.processEvents();
    QSettings userLayout(QStringLiteral("AeroSimWorkbench"), QStringLiteral("UiLayout"));
    if (!check(restoreUserLayout->isEnabled()
               && userLayout.value("UserLayout/version").toInt() == 1,
               "Saving a user layout did not create the single persistent record")) return 82;
    const double savedMeshRatio = userLayout.value("UserLayout/meshLayout").toMap()
        .value("objectPanelRatio").toDouble();
    const double savedMonitorRatio = userLayout.value("UserLayout/monitorCurveRatio").toDouble();
    if (!check(savedMeshRatio >= 0.20 && savedMeshRatio <= 0.42
               && savedMonitorRatio >= 0.45 && savedMonitorRatio <= 0.82,
               "Saved layout ratios are outside the supported adaptive range")) return 85;
    meshMainSplitter->setSizes({245, 755});
    monitorLayoutSplitter->setSizes({470, 530});
    collapseOutput->click();
    window.ribbonBar()->setMinimumMode(true);
    restoreUserLayout->trigger();
    app.processEvents();
    const QList<int> restoredMeshSizes = meshMainSplitter->sizes();
    const QList<int> restoredMonitorSizes = monitorLayoutSplitter->sizes();
    if (!check(restoredMeshSizes.value(0) >= 245 && restoredMeshSizes.value(1) > 0
               && restoredMonitorSizes.value(0) > 0 && restoredMonitorSizes.value(1) >= 130
               && !window.ribbonBar()->isMinimumMode()
               && !window.findChild<QWidget *>("outputBody")->isHidden(),
               "Restoring the user layout did not recover relative split and collapse state")) return 83;
    restoreDefaultLayout->trigger();
    app.processEvents();
    const QList<int> defaultMeshSizes = meshMainSplitter->sizes();
    const double defaultMeshRatio = double(defaultMeshSizes.value(0))
        / qMax(1, defaultMeshSizes.value(0) + defaultMeshSizes.value(1));
    if (!check(defaultMeshRatio >= (290.0 / 1140.0) - 0.04 && defaultMeshRatio <= 0.42
               && window.findChild<QWidget *>("outputBody")->isHidden()
               && userLayout.value("UserLayout/version").toInt() == 1,
               "Default layout must be safe without deleting the saved user layout")) return 84;
    const QString lightStyle = window.styleSheet();
    auto *themeMenu = window.findChild<QMenu *>("themeMenu");
    if (!check(themeMenu && !themeMenu->menuAction()->isEnabled()
               && window.property("windowThemeId").toString() == "office-blue"
               && window.ribbonBar()->windowTitleBackgroundBrush().color() == QColor("#2b579a")
               && window.palette().color(QPalette::Window) == QColor("#f4f7fb"),
               "Main window must use the fixed default light theme")) return 23;
    for (auto *action : themeMenu->actions()) {
        if (!check(!action->isEnabled() && action->isChecked() == (action->data().toString() == "office-blue"),
                   "Theme switch actions must be disabled with only the default checked")) return 24;
    }
    const QString screenshots = qEnvironmentVariable("AEROSIM_THEME_SCREENSHOTS");
    if (!screenshots.isEmpty()) {
        QDir().mkpath(screenshots);
        window.show();
        app.processEvents();
        window.grab().save(QDir(screenshots).filePath("office-blue.png"));
    }
    // Check migration of old preferences without rendering any alternate theme.
    for (const QString &legacy : {QStringLiteral("dark"), QStringLiteral("dark-gray"),
                                  QStringLiteral("office-light"), QStringLiteral("unknown-theme")}) {
        QSettings saved(QStringLiteral("AeroSimWorkbench"), QStringLiteral("Appearance"));
        saved.setValue("theme", legacy);
        aerosim::MainWindow reopened;
        app.processEvents();
        if (!check(reopened.property("windowThemeId").toString() == "office-blue"
                   && reopened.palette().color(QPalette::Window) == QColor("#f4f7fb")
                   && saved.value("theme").toString() == "office-blue"
                   && window.styleSheet() == lightStyle,
                   "Stored alternate themes must be normalized to the default light mode")) return 26;
    }
    for (auto slot : {aerosim::WidgetSlot::Viewport, aerosim::WidgetSlot::Log, aerosim::WidgetSlot::Error, aerosim::WidgetSlot::Monitor}) {
        auto *widget = new QLabel("Injected component");
        if (!check(window.insertWidget(slot, widget) && window.isAncestorOf(widget), "Widget ownership not transferred")) return 2;
        auto *duplicate = new QLabel;
        bool rejected = !window.insertWidget(slot, duplicate) && !duplicate->parent();
        delete duplicate;
        if (!check(rejected, "Occupied slot must reject without taking ownership")) return 3;
    }
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
    QTreeWidgetItem *taskRow = nullptr;
    auto *taskTree = window.findChild<QTreeWidget *>("taskTree");
    for (int row = 0; row < taskTree->topLevelItemCount(); ++row)
        if (taskTree->topLevelItem(row)->text(0) == "test-task") taskRow = taskTree->topLevelItem(row);
    if (!check(taskRow && taskTree->topLevelItemCount() == 4, "Task rows not updated by id")) return 7;
    QTreeWidgetItem *navigationTask = nullptr;
    for (QTreeWidgetItemIterator it(projectTree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole + 1).toString() == "test-task") navigationTask = *it;
    if (!check(navigationTask && navigationTask->data(0, Qt::UserRole + 3).toInt()
               == static_cast<int>(aerosim::TaskStatus::Finished),
               "Task event did not locally update structured monitor status")) return 40;
    window.findChild<QPushButton *>("clearOutputButton")->click();
    if (!check(outputTable->rowCount() == 0, "Clear Output must only clear the UI rows")) return 50;
    window.bindServices({});
    if (!check(!run->isEnabled(), "Unbinding must disable run")) return 8;
    projectTree->setCurrentItem(parameterNode->child(1));
    auto *boundaryTable = window.findChild<QTreeWidget *>("boundaryTable");
    if (!check(boundaryTable && boundaryTable->topLevelItemCount() == 5
               && workspace->currentWidget()->objectName() == "parameterEditor_boundary-conditions"
               && window.findChild<QDockWidget *>("propertyDock")->isHidden(),
               "Merged boundary page must remain embedded")) return 90;
    auto *boundaryPanel = workspace->currentWidget()->findChild<aerosim::BoundaryConditionsPanel *>();
    auto *boundaryType = boundaryPanel->findChild<QComboBox *>("boundaryType");
    auto *applyBoundary = boundaryPanel->findChild<QPushButton *>("applyBoundary");
    auto *editedBoundary = boundaryTable->currentItem();
    const QString originalBoundaryType = editedBoundary->data(1, Qt::UserRole).toString();
    const QString editedBoundaryId = editedBoundary->data(0, Qt::UserRole).toString();
    if (!check(boundaryType->count() == 16 && boundaryType->findData("symmetry") >= 0
               && boundaryType->findData("inviscid-wall") >= 0 && boundaryType->findData("farfield") == 0,
               "Boundary types must include all reference choices and preserve existing identifiers")) return 122;
    QVariantMap boundaryDraft;
    QObject::connect(boundaryPanel, &aerosim::BoundaryConditionsPanel::draftChanged, &window,
                     [&boundaryDraft](const QVariantMap &draft) { boundaryDraft = draft; });
    for (int index = 0; index < boundaryType->count(); ++index) {
        boundaryType->setCurrentIndex(index);
        if (applyBoundary->isEnabled()) applyBoundary->click();
        const QString typeId = boundaryType->itemData(index).toString();
        const QString typeText = boundaryType->itemText(index);
        if (!check(editedBoundary->data(1, Qt::UserRole).toString() == typeId
                   && editedBoundary->text(1) == typeText
                   && editedBoundary->data(0, Qt::UserRole).toString() == editedBoundaryId
                   && !applyBoundary->isEnabled(),
                   "Applying a boundary type must update its label without changing boundary identity")) return 123;
        aerosim::BoundaryConditionsPanel restoredBoundary(boundaryDraft);
        auto *restoredType = restoredBoundary.findChild<QComboBox *>("boundaryType");
        if (!check(restoredType->currentData().toString() == typeId && restoredType->currentText() == typeText,
                   "Every boundary type must survive draft restoration")) return 124;
    }
    boundaryType->setCurrentIndex(boundaryType->findData(originalBoundaryType));
    applyBoundary->click();
    auto *farfieldParameters = boundaryPanel->findChild<aerosim::FarfieldParametersPanel *>();
    auto *farfieldType = farfieldParameters->findChild<QComboBox *>("farfieldParameterType");
    QTreeWidgetItem *farfieldBoundary = nullptr;
    for (int row = 0; row < boundaryTable->topLevelItemCount(); ++row)
        if (boundaryTable->topLevelItem(row)->data(0, Qt::UserRole) == "farfield")
            farfieldBoundary = boundaryTable->topLevelItem(row);
    boundaryTable->setCurrentItem(farfieldBoundary);
    const auto farfieldNumber = [farfieldParameters](const QString &key) {
        return farfieldParameters->findChild<QDoubleSpinBox *>(QStringLiteral("farfield_") + key);
    };
    if (!check(farfieldBoundary && !farfieldParameters->parentWidget()->isHidden()
               && farfieldType->count() == 4 && farfieldType->currentData() == "static-temperature-pressure"
               && farfieldNumber("machNumber")->value() == 3.0
               && farfieldNumber("staticPressure")->value() == 955.0
               && farfieldNumber("staticTemperature")->value() == 60.0,
               "Farfield defaults must match reference and appear only for farfield boundaries")) return 125;
    auto *farfieldForm = qobject_cast<QFormLayout *>(farfieldParameters->layout());
    const QStringList farfieldKeys = {"unitReynoldsNumber", "staticPressure", "staticTemperature",
                                     "totalPressure", "totalTemperature", "altitude"};
    const QList<QStringList> farfieldVisible = {{"unitReynoldsNumber", "staticTemperature"},
        {"staticPressure", "staticTemperature"}, {"totalPressure", "totalTemperature"}, {"altitude"}};
    for (int index = 0; index < 4; ++index) {
        farfieldType->setCurrentIndex(index);
        for (const auto &key : farfieldKeys) {
            auto *field = farfieldNumber(key);
            const bool visible = farfieldVisible[index].contains(key);
            if (!check(field->isHidden() != visible
                       && (visible ? !farfieldForm->labelForField(field)->isHidden()
                                   : farfieldForm->indexOf(field) == -1),
                       "Farfield type must show the matching fields without empty rows")) return 126;
        }
    }
    farfieldType->setCurrentIndex(1);
    farfieldNumber("staticTemperature")->setValue(280.125);
    farfieldNumber("angleOfAttack")->setValue(5.5);
    farfieldNumber("sideslipAngle")->setValue(-2.0);
    farfieldType->setCurrentIndex(2);
    farfieldNumber("totalPressure")->setValue(12345.5);
    farfieldType->setCurrentIndex(1);
    applyBoundary->click();
    QVariantMap appliedFarfield;
    for (const auto &value : boundaryDraft.value("boundaries").toList()) {
        const auto row = value.toMap();
        if (row.value("id") == "farfield") appliedFarfield = row.value("farfield").toMap();
    }
    if (!check(appliedFarfield.value("staticTemperature").toDouble() == 280.125
               && appliedFarfield.value("totalPressure").toDouble() == 12345.5
               && appliedFarfield.value("angleOfAttack").toDouble() == 5.5
               && appliedFarfield.value("sideslipAngle").toDouble() == -2.0
               && !applyBoundary->isEnabled(),
               "Applying farfield must save active and hidden values to its own boundary")) return 127;
    farfieldNumber("staticTemperature")->setValue(300.0);
    boundaryTable->setCurrentItem(editedBoundary);
    if (!check(farfieldParameters->parentWidget()->isHidden(),
               "Non-farfield boundary must hide farfield settings")) return 128;
    boundaryType->setCurrentIndex(boundaryType->findData("farfield"));
    if (!check(farfieldNumber("staticTemperature")->value() == 60.0,
               "Editing one boundary must not leak values into another boundary")) return 129;
    boundaryTable->setCurrentItem(farfieldBoundary);
    if (!check(farfieldNumber("staticTemperature")->value() == 280.125 && !applyBoundary->isEnabled(),
               "Switching boundaries must discard uncommitted values and restore applied ones")) return 130;
    boundaryType->setCurrentIndex(boundaryType->findData("symmetry"));
    if (!check(farfieldParameters->parentWidget()->isHidden(),
               "Changing farfield to symmetry must hide its flow editor")) return 131;
    boundaryType->setCurrentIndex(boundaryType->findData("farfield"));
    if (!check(farfieldNumber("staticTemperature")->value() == 280.125 && !applyBoundary->isEnabled(),
               "Switching boundary type back must retain entered flow values")) return 132;
    {
        aerosim::BoundaryConditionsPanel restoredFarfield(boundaryDraft);
        if (!check(restoredFarfield.findChild<QDoubleSpinBox *>("farfield_staticTemperature")->value() == 280.125
                   && restoredFarfield.findChild<QDoubleSpinBox *>("farfield_totalPressure")->value() == 12345.5,
                   "Farfield drafts must restore all fields")) return 133;
    }
    // Capture the supplied example rather than test edits.
    farfieldParameters->setParameters({});
    farfieldNumber("staticTemperature")->setValue(60.1);
    farfieldNumber("staticTemperature")->setValue(60.0);
    applyBoundary->click();
    projectTree->setCurrentItem(parameterNode->child(2));
    // Boundary-specific thermal and mass-flow settings use independent drafts.
    projectTree->setCurrentItem(parameterNode->child(1));
    boundaryTable->setCurrentItem(editedBoundary);
    auto *wallTemperature = boundaryPanel->findChild<QDoubleSpinBox *>("boundary_wallTemperature");
    auto *massFlow = boundaryPanel->findChild<QDoubleSpinBox *>("boundary_massFlowRate");
    auto *inletTemperature = boundaryPanel->findChild<QDoubleSpinBox *>("boundary_totalTemperature");
    boundaryType->setCurrentIndex(boundaryType->findData("no-slip-isothermal-wall"));
    if (!check(!wallTemperature->isHidden() && wallTemperature->value() == 300.0
               && massFlow->isHidden() && inletTemperature->isHidden(),
               "Isothermal wall must show only wall temperature with reference default")) return 134;
    wallTemperature->setValue(325.125);
    applyBoundary->click();
    boundaryType->setCurrentIndex(boundaryType->findData("mass-flow-subsonic-outlet"));
    if (!check(wallTemperature->isHidden() && !massFlow->isHidden() && massFlow->value() == 100.0
               && inletTemperature->isHidden(),
               "Mass-flow outlet must show only mass flow")) return 135;
    massFlow->setValue(125.25);
    applyBoundary->click();
    boundaryType->setCurrentIndex(boundaryType->findData("mass-flow-subsonic-inlet"));
    if (!check(!massFlow->isHidden() && !inletTemperature->isHidden() && wallTemperature->isHidden()
               && massFlow->value() == 100.0 && inletTemperature->value() == 300.0,
               "Mass-flow inlet must have independent mass flow and total temperature defaults")) return 136;
    massFlow->setValue(88.5);
    inletTemperature->setValue(340.125);
    applyBoundary->click();
    boundaryType->setCurrentIndex(boundaryType->findData("no-slip-isothermal-wall"));
    if (!check(wallTemperature->value() == 325.125, "Switching type must retain wall temperature")) return 137;
    boundaryType->setCurrentIndex(boundaryType->findData("mass-flow-subsonic-outlet"));
    if (!check(massFlow->value() == 125.25, "Outlet and inlet drafts must remain independent")) return 138;
    boundaryType->setCurrentIndex(boundaryType->findData("mass-flow-subsonic-inlet"));
    inletTemperature->setValue(400.0);
    boundaryTable->setCurrentItem(farfieldBoundary);
    boundaryTable->setCurrentItem(editedBoundary);
    if (!check(massFlow->value() == 88.5 && inletTemperature->value() == 340.125 && !applyBoundary->isEnabled(),
               "Boundary switching must restore applied settings and discard uncommitted edits")) return 139;
    {
        aerosim::BoundaryConditionsPanel restoredThermal(boundaryDraft);
        if (!check(restoredThermal.findChild<QDoubleSpinBox *>("boundary_massFlowRate")->value() == 88.5
                   && restoredThermal.findChild<QDoubleSpinBox *>("boundary_totalTemperature")->value() == 340.125,
                   "Inlet settings must survive complete draft restoration")) return 140;
    }
    boundaryType->setCurrentIndex(boundaryType->findData("symmetry"));
    if (!check(wallTemperature->isHidden() && massFlow->isHidden() && inletTemperature->isHidden(),
               "Unrelated boundary types must hide thermal and mass-flow fields")) return 141;
    boundaryType->setCurrentIndex(boundaryType->findData("no-slip-isothermal-wall-wall-function"));
    if (!check(!wallTemperature->isHidden() && massFlow->isHidden(),
               "Isothermal wall function variant must expose temperature")) return 142;
    const QStringList pressureTypes = {"pressure-subsonic-outlet", "pressure-subsonic-inlet", "mixed-pressure-outlet"};
    const QList<QStringList> pressureKeys = {{"outletStaticPressure"},
        {"pressureRelaxationFactor", "inletTemperature", "inletTotalPressure"},
        {"pressureRelaxationFactor", "outletStaticPressure"}};
    const QVariantMap pressureDefaults = {{"outletStaticPressure", 1000.0}, {"pressureRelaxationFactor", 1.0},
        {"inletTemperature", 300.0}, {"inletTotalPressure", 1000.0}};
    const QVariantMap pressureEdits = {{"outletStaticPressure", 1200.125}, {"pressureRelaxationFactor", 0.625},
        {"inletTemperature", 315.125}, {"inletTotalPressure", 2100.5}};
    const QStringList allBoundaryFields = {"wallTemperature", "massFlowRate", "totalTemperature",
        "outletStaticPressure", "pressureRelaxationFactor", "inletTemperature", "inletTotalPressure"};
    for (int index = 0; index < pressureTypes.size(); ++index) {
        boundaryType->setCurrentIndex(boundaryType->findData(pressureTypes[index]));
        for (const auto &key : allBoundaryFields) {
            auto *number = boundaryPanel->findChild<QDoubleSpinBox *>(QStringLiteral("boundary_") + key);
            auto *form = qobject_cast<QFormLayout *>(number->parentWidget()->layout());
            const bool visible = pressureKeys[index].contains(key);
            if (!check(number->isHidden() != visible
                       && (visible ? !form->labelForField(number)->isHidden() : form->indexOf(number) == -1),
                       "Pressure boundary must show only matching fields and labels")) return 143;
            if (visible) {
                if (!check(number->value() == pressureDefaults.value(key).toDouble(),
                           "Pressure defaults must match supplied examples independently for each type")) return 144;
                number->setValue(pressureEdits.value(key).toDouble());
            }
        }
        applyBoundary->click();
        if (!check(!applyBoundary->isEnabled(), "Applied pressure settings must clear pending state")) return 145;
        aerosim::BoundaryConditionsPanel restoredPressure(boundaryDraft);
        for (const auto &key : pressureKeys[index]) {
            auto *number = restoredPressure.findChild<QDoubleSpinBox *>(QStringLiteral("boundary_") + key);
            if (!check(number->value() == pressureEdits.value(key).toDouble(),
                       "Pressure settings must restore all values with their precision")) return 146;
        }
    }
    auto *outletPressure = boundaryPanel->findChild<QDoubleSpinBox *>("boundary_outletStaticPressure");
    auto *relaxation = boundaryPanel->findChild<QDoubleSpinBox *>("boundary_pressureRelaxationFactor");
    boundaryType->setCurrentIndex(boundaryType->findData("pressure-subsonic-outlet"));
    outletPressure->setValue(1500.5);
    applyBoundary->click();
    boundaryType->setCurrentIndex(boundaryType->findData("mixed-pressure-outlet"));
    if (!check(outletPressure->value() == 1200.125 && relaxation->value() == 0.625,
               "Mixed and subsonic outlet pressure must be stored independently")) return 147;
    outletPressure->setValue(2000.0);
    boundaryTable->setCurrentItem(farfieldBoundary);
    boundaryType->setCurrentIndex(boundaryType->findData("mixed-pressure-outlet"));
    if (!check(outletPressure->value() == 1000.0 && relaxation->value() == 1.0,
               "Pressure values must not leak across boundaries")) return 148;
    boundaryTable->setCurrentItem(editedBoundary);
    boundaryType->setCurrentIndex(boundaryType->findData("mixed-pressure-outlet"));
    if (!check(outletPressure->value() == 1200.125 && relaxation->value() == 0.625,
               "Boundary switching must discard unapplied pressure edits")) return 149;
    boundaryType->setCurrentIndex(boundaryType->findData("symmetry"));
    if (!check(outletPressure->isHidden() && relaxation->isHidden(),
               "Other boundary types must hide pressure fields")) return 150;
    boundaryTable->setCurrentItem(farfieldBoundary);
    projectTree->setCurrentItem(parameterNode->child(2));
    auto *motionEnabled = window.findChild<QCheckBox *>("meshMotionEnabled");
    auto *motionCount = window.findChild<QSpinBox *>("motionBodyCount");
    if (!check(motionEnabled && motionCount && !motionEnabled->isChecked() && motionCount->value() == 0,
               "Merged mesh motion defaults changed")) return 91;
    motionEnabled->setChecked(true);
    motionCount->setValue(2);
    auto *motionPanel = window.findChild<aerosim::MeshMotionPanel *>();
    QVariantMap forcedDraft;
    QObject::connect(motionPanel, &aerosim::MeshMotionPanel::draftChanged, &window,
                     [&forcedDraft](const QVariantMap &draft) { forcedDraft = draft; });
    bool forcedDialogChecked = false;
    const QString motionCaptures = qEnvironmentVariable("AEROSIM_PARAMETER_SCREENSHOTS");
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::ForcedMotionDialog *>();
        if (!dialog) return;
        auto *rotation = dialog->findChild<QComboBox *>("rotationType");
        auto *frequencyType = dialog->findChild<QComboBox *>("frequencyType");
        forcedDialogChecked = rotation->currentData() == "none"
            && dialog->findChild<QWidget *>("rotationSettings")->isHidden();
        const auto capture = [&](const QString &name) {
            if (motionCaptures.isEmpty()) return;
            QDir().mkpath(motionCaptures);
            app.processEvents();
            dialog->grab().save(QDir(motionCaptures).filePath(name));
        };
        capture("forced-none.png");
        rotation->setCurrentIndex(1);
        forcedDialogChecked = forcedDialogChecked
            && !dialog->findChild<QWidget *>("constantRotationSettings")->isHidden()
            && dialog->findChild<QDoubleSpinBox *>("rotationAxis1")->value() == 1.0;
        capture("forced-constant.png");
        rotation->setCurrentIndex(2);
        auto *frequency = dialog->findChild<QDoubleSpinBox *>("rotationFrequency");
        auto *reduced = dialog->findChild<QDoubleSpinBox *>("reducedFrequency");
        frequency->setValue(2.5);
        capture("forced-sine-frequency.png");
        frequencyType->setCurrentIndex(1);
        reduced->setValue(0.25);
        forcedDialogChecked = forcedDialogChecked && frequency->isHidden() && !reduced->isHidden();
        capture("forced-sine-reduced.png");
        frequencyType->setCurrentIndex(0);
        forcedDialogChecked = forcedDialogChecked && frequency->value() == 2.5;
        frequencyType->setCurrentIndex(1);
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    auto forcedParameters = forcedDraft.value("bodies").toList().value(0).toMap().value("forcedMotion").toMap();
    if (!check(forcedDialogChecked && forcedParameters.value("rotationType") == "sinusoidal"
               && forcedParameters.value("rotationFrequency").toDouble() == 2.5
               && forcedParameters.value("reducedFrequency").toDouble() == 0.25
               && forcedParameters.value("rotationDuration").toDouble() == 1e99,
               "Forced motion dialog must switch fields and commit complete parameters")) return 151;
    bool restoredForced = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::ForcedMotionDialog *>();
        if (!dialog) return;
        restoredForced = dialog->findChild<QComboBox *>("rotationType")->currentData() == "sinusoidal"
            && dialog->findChild<QDoubleSpinBox *>("reducedFrequency")->value() == 0.25;
        dialog->findChild<QDoubleSpinBox *>("reducedFrequency")->setValue(0.5);
        dialog->reject();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    if (!check(restoredForced && forcedDraft.value("bodies").toList().value(0).toMap().value("forcedMotion").toMap() == forcedParameters,
               "Cancel must preserve the original forced motion parameters")) return 152;
    auto *motionType = window.findChild<QComboBox *>("motionBody0_type");
    motionType->setCurrentIndex(motionType->findData("fixed-axis"));
    bool fixedAxisChecked = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::FixedAxisMotionDialog *>();
        if (!dialog) return;
        auto *definition = dialog->findChild<QComboBox *>("axisDefinition");
        auto *count = dialog->findChild<QSpinBox *>("externalForceCount");
        auto *current = dialog->findChild<QComboBox *>("currentExternalForce");
        auto *points = dialog->findChild<QSpinBox *>("externalForcePointCount");
        auto *table = dialog->findChild<QTableWidget *>("externalForceTable");
        const auto capture = [&](const QString &name) {
            if (motionCaptures.isEmpty()) return;
            app.processEvents(); dialog->grab().save(QDir(motionCaptures).filePath(name));
        };
        fixedAxisChecked = definition->currentData() == "direction" && count->value() == 1
            && points->value() == 2 && table->columnCount() == 7
            && dialog->findChild<QDoubleSpinBox *>("momentOfInertia")->value() == 1.0;
        capture("fixed-axis-direction.png");
        definition->setCurrentIndex(1);
        capture("fixed-axis-two-points.png");
        count->setValue(2);
        qobject_cast<QDoubleSpinBox *>(table->cellWidget(0, 0))->setValue(12.5);
        dialog->findChild<QPushButton *>("saveExternalForce")->click();
        current->setCurrentIndex(1);
        qobject_cast<QDoubleSpinBox *>(table->cellWidget(1, 1))->setValue(24.5);
        dialog->findChild<QComboBox *>("externalForceFrame")->setCurrentIndex(1);
        points->setValue(3);
        points->setValue(1);
        points->setValue(3);
        fixedAxisChecked = fixedAxisChecked && qobject_cast<QDoubleSpinBox *>(table->cellWidget(1, 1))->value() == 24.5;
        current->setCurrentIndex(0);
        fixedAxisChecked = fixedAxisChecked && qobject_cast<QDoubleSpinBox *>(table->cellWidget(0, 0))->value() == 12.5;
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    const auto fixedParameters = forcedDraft.value("bodies").toList()[0].toMap().value("fixedAxisMotion").toMap();
    if (!check(fixedAxisChecked && fixedParameters.value("axisDefinition") == "two-points"
               && fixedParameters.value("forceCount").toInt() == 2
               && fixedParameters.value("forces").toList()[1].toMap().value("frame") == "body",
               "Fixed axis dialog must preserve independent force rows, frame and axis definition")) return 153;
    bool fixedRestored = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::FixedAxisMotionDialog *>();
        if (!dialog) return;
        fixedRestored = dialog->findChild<QComboBox *>("axisDefinition")->currentData() == "two-points"
            && dialog->findChild<QSpinBox *>("externalForceCount")->value() == 2;
        dialog->findChild<QDoubleSpinBox *>("momentOfInertia")->setValue(99);
        dialog->reject();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    if (!check(fixedRestored && forcedDraft.value("bodies").toList()[0].toMap().value("fixedAxisMotion").toMap() == fixedParameters,
               "Fixed axis cancel must preserve committed parameters")) return 154;
    motionType->setCurrentIndex(motionType->findData("six-dof"));
    bool sixDofChecked = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::SixDofMotionDialog *>("sixDofMotionDialog");
        if (!dialog) return;
        auto *forces = dialog->findChild<QTableWidget *>("externalForceTable");
        auto *moments = dialog->findChild<QTableWidget *>("externalMomentTable");
        auto *forceCount = dialog->findChild<QSpinBox *>("externalForceCount");
        auto *forceCurrent = dialog->findChild<QComboBox *>("externalForceCurrent");
        auto *momentCount = dialog->findChild<QSpinBox *>("externalMomentCount");
        auto *momentCurrent = dialog->findChild<QComboBox *>("externalMomentCurrent");
        sixDofChecked = dialog->findChild<QDoubleSpinBox *>("mass")->value() == 1.
            && dialog->findChild<QDoubleSpinBox *>("gravityDirection2")->value() == -1.
            && dialog->findChild<QDoubleSpinBox *>("gravityMagnitude")->value() == 9.8
            && forces->columnCount() == 7 && moments->columnCount() == 4
            && forces->rowCount() == 1 && moments->rowCount() == 1;
        if (!motionCaptures.isEmpty()) {
            app.processEvents(); dialog->grab().save(QDir(motionCaptures).filePath("six-dof-top.png"));
            auto *scroll = dialog->findChild<QScrollArea *>("sixDofScroll");
            scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            app.processEvents(); dialog->grab().save(QDir(motionCaptures).filePath("six-dof-bottom.png"));
        }
        dialog->findChild<QDoubleSpinBox *>("mass")->setValue(3.5);
        dialog->findChild<QCheckBox *>("ignoreTranslation0")->setChecked(true);
        dialog->findChild<QCheckBox *>("ignoreRotation2")->setChecked(true);
        forceCount->setValue(2);
        qobject_cast<QDoubleSpinBox *>(forces->cellWidget(0, 0))->setValue(12.5);
        dialog->findChild<QPushButton *>("externalForceSave")->click();
        forceCurrent->setCurrentIndex(1);
        qobject_cast<QDoubleSpinBox *>(forces->cellWidget(0, 1))->setValue(24.5);
        dialog->findChild<QComboBox *>("externalForceFrame")->setCurrentIndex(1);
        forceCount->setValue(0); forceCount->setValue(2); forceCurrent->setCurrentIndex(1);
        sixDofChecked = sixDofChecked && qobject_cast<QDoubleSpinBox *>(forces->cellWidget(0, 1))->value() == 24.5;
        momentCount->setValue(2);
        qobject_cast<QDoubleSpinBox *>(moments->cellWidget(0, 0))->setValue(7.5);
        momentCurrent->setCurrentIndex(1);
        auto *pointCount = dialog->findChild<QSpinBox *>("externalMomentPointCount");
        pointCount->setValue(3);
        qobject_cast<QDoubleSpinBox *>(moments->cellWidget(2, 2))->setValue(-9.5);
        pointCount->setValue(1); pointCount->setValue(3);
        sixDofChecked = sixDofChecked && qobject_cast<QDoubleSpinBox *>(moments->cellWidget(2, 2))->value() == -9.5;
        momentCurrent->setCurrentIndex(0);
        sixDofChecked = sixDofChecked && qobject_cast<QDoubleSpinBox *>(moments->cellWidget(0, 0))->value() == 7.5;
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    const auto sixDofParameters = forcedDraft.value("bodies").toList()[0].toMap().value("sixDofMotion").toMap();
    if (!check(sixDofChecked && sixDofParameters.value("mass").toDouble() == 3.5
               && sixDofParameters.value("ignoreTranslation0").toBool() && sixDofParameters.value("ignoreRotation2").toBool()
               && sixDofParameters.value("forces").toList()[1].toMap().value("frame") == "body"
               && sixDofParameters.value("moments").toList().size() == 2,
               "Six DOF must preserve constraints and independent force/moment drafts")) return 155;
    bool sixDofRestored = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::SixDofMotionDialog *>("sixDofMotionDialog");
        if (!dialog) return;
        sixDofRestored = dialog->findChild<QDoubleSpinBox *>("mass")->value() == 3.5
            && dialog->findChild<QCheckBox *>("ignoreRotation2")->isChecked();
        dialog->findChild<QDoubleSpinBox *>("mass")->setValue(99.);
        dialog->findChild<QSpinBox *>("externalMomentCount")->setValue(0);
        dialog->reject();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    if (!check(sixDofRestored && forcedDraft.value("bodies").toList()[0].toMap().value("sixDofMotion").toMap() == sixDofParameters,
               "Six DOF cancel must preserve committed parameters")) return 156;
    motionType->setCurrentIndex(motionType->findData("rotation-release"));
    bool releaseChecked = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::SixDofMotionDialog *>("rotationReleaseMotionDialog");
        if (!dialog) return;
        auto *definition = dialog->findChild<QComboBox *>("axisDefinition");
        auto *forceCount = dialog->findChild<QSpinBox *>("externalForceCount");
        auto *momentCount = dialog->findChild<QSpinBox *>("externalMomentCount");
        auto *forceTable = dialog->findChild<QTableWidget *>("externalForceTable");
        auto *momentTable = dialog->findChild<QTableWidget *>("externalMomentTable");
        auto *angle = dialog->findChild<QDoubleSpinBox *>("maximumRotationAngle");
        releaseChecked = definition->currentData() == "direction" && angle->value() == 0.
            && dialog->findChild<QDoubleSpinBox *>("momentOfInertia")->value() == 1.
            && forceCount->value() == 0 && momentCount->value() == 0
            && !forceTable->isVisible() && !momentTable->isVisible()
            && !dialog->findChild<QDoubleSpinBox *>("initialVelocity0");
        if (!motionCaptures.isEmpty()) {
            app.processEvents(); dialog->grab().save(QDir(motionCaptures).filePath("rotation-release-top.png"));
            auto *scroll = dialog->findChild<QScrollArea *>("sixDofScroll");
            scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            app.processEvents(); dialog->grab().save(QDir(motionCaptures).filePath("rotation-release-bottom.png"));
        }
        auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        dialog->findChild<QDoubleSpinBox *>("axisDirection0")->setValue(0.);
        ok->click();
        releaseChecked = releaseChecked && dialog->isVisible() && !dialog->findChild<QLabel *>("motionValidationError")->isHidden();
        definition->setCurrentIndex(1);
        angle->setValue(45.);
        dialog->findChild<QCheckBox *>("ignoreAerodynamicMoment")->setChecked(true);
        dialog->findChild<QDoubleSpinBox *>("mass")->setValue(2.5);
        forceCount->setValue(1); momentCount->setValue(1);
        releaseChecked = releaseChecked && forceTable->isVisible() && momentTable->isVisible();
        qobject_cast<QDoubleSpinBox *>(forceTable->cellWidget(0, 0))->setValue(8.);
        qobject_cast<QDoubleSpinBox *>(momentTable->cellWidget(0, 2))->setValue(6.);
        forceCount->setValue(0); momentCount->setValue(0);
        releaseChecked = releaseChecked && !forceTable->isVisible() && !momentTable->isVisible();
        forceCount->setValue(1); momentCount->setValue(1);
        releaseChecked = releaseChecked && qobject_cast<QDoubleSpinBox *>(forceTable->cellWidget(0, 0))->value() == 8.
            && qobject_cast<QDoubleSpinBox *>(momentTable->cellWidget(0, 2))->value() == 6.;
        ok->click();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    const auto releaseParameters = forcedDraft.value("bodies").toList()[0].toMap().value("rotationReleaseMotion").toMap();
    if (!check(releaseChecked && releaseParameters.value("axisDefinition") == "two-points"
               && releaseParameters.value("maximumRotationAngle").toDouble() == 45.
               && releaseParameters.value("mass").toDouble() == 2.5
               && releaseParameters.value("ignoreAerodynamicMoment").toBool(),
               "Rotation release must validate axis and preserve independent settings")) return 157;
    bool releaseRestored = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = window.findChild<aerosim::SixDofMotionDialog *>("rotationReleaseMotionDialog");
        if (!dialog) return;
        releaseRestored = dialog->findChild<QDoubleSpinBox *>("maximumRotationAngle")->value() == 45.
            && dialog->findChild<QComboBox *>("axisDefinition")->currentData() == "two-points";
        dialog->findChild<QDoubleSpinBox *>("mass")->setValue(99.); dialog->reject();
    });
    window.findChild<QPushButton *>("motionBody0_settings")->click();
    if (!check(releaseRestored && forcedDraft.value("bodies").toList()[0].toMap().value("rotationReleaseMotion").toMap() == releaseParameters,
               "Rotation release cancel must preserve committed parameters")) return 158;
    projectTree->setCurrentItem(parameterNode->child(0));
    auto *calculationType = window.findChild<QRadioButton *>("calculationType_steady");
    calculationType->setChecked(true);
    projectTree->setCurrentItem(parameterNode->child(2));
    if (!check(motionCount->value() == 2 && window.findChild<QTableWidget *>("motionBodyTable")->rowCount() == 2,
               "Merged navigation lost motion draft")) return 92;
    projectTree->setCurrentItem(parameterNode->child(0));
    if (!check(calculationType->isChecked(),
               "Merged navigation lost equation draft")) return 93;
    auto *equationPanel = window.findChild<aerosim::GoverningEquationsPanel *>();
    auto *laminar = equationPanel->findChild<QRadioButton *>("laminarFlow");
    auto *turbulent = equationPanel->findChild<QRadioButton *>("turbulentFlow");
    auto *turbulenceModel = equationPanel->findChild<QRadioButton *>("turbulenceModel_sa");
    auto *rarefied = equationPanel->findChild<QCheckBox *>("rarefiedGasEffect");
    auto *knudsen = equationPanel->findChild<QDoubleSpinBox *>("knudsenNumber");
    auto *freestreamParticles = equationPanel->findChild<QSpinBox *>("freestreamParticleCount");
    auto *gridParticles = equationPanel->findChild<QSpinBox *>("gridParticleCount");
    auto *collisionModel = equationPanel->findChild<QRadioButton *>("collisionModel_vss");
    if (!check(laminar && turbulent && turbulent->isChecked() && turbulenceModel->isVisibleTo(equationPanel)
               && turbulenceModel->isChecked() && rarefied->isChecked()
               && knudsen->value() == 1.0 && freestreamParticles->value() == 100 && gridParticles->value() == 2
               && collisionModel->isChecked() && equationPanel->findChildren<QComboBox *>().isEmpty(),
               "Expanded equation defaults do not match reference")) return 95;
    QVariantMap equationDraft;
    QObject::connect(equationPanel, &aerosim::GoverningEquationsPanel::draftChanged, &window,
                     [&equationDraft](const QVariantMap &draft) { equationDraft = draft; });
    equationPanel->findChild<QRadioButton *>("calculationType_unsteady")->setChecked(true);
    if (!check(!calculationType->isChecked() && equationDraft.value("calculationType") == "unsteady",
               "Calculation type radios must be exclusive and publish the draft")) return 102;
    calculationType->setChecked(true);
    laminar->setChecked(true);
    if (!check(!turbulenceModel->isVisibleTo(equationPanel) && equationDraft.value("flowRegime") == "laminar",
               "Laminar flow must hide turbulence settings")) return 96;
    equationPanel->findChild<QRadioButton *>("eulerEquation")->setChecked(true);
    if (!check(equationDraft.value("flowRegime") == "inviscid" && laminar->parentWidget()->isHidden(),
               "Euler must use inviscid flow without losing viscous choices")) return 97;
    equationPanel->findChild<QRadioButton *>("navierStokesEquation")->setChecked(true);
    if (!check(laminar->isChecked() && equationDraft.value("flowRegime") == "laminar",
               "Returning to N-S must restore the previous flow choice")) return 98;
    turbulent->setChecked(true);
    auto *turbulenceOptions = equationPanel->findChild<QWidget *>("turbulenceModel");
    const QStringList turbulenceTokens = {"sa", "kw-menter", "des", "ddes", "wilcox-kw1998", "wilcox-kw2006"};
    if (!check(turbulenceOptions->findChildren<QRadioButton *>().size() == 6,
               "Turbulence models must match the six reference options")) return 104;
    for (const auto &token : turbulenceTokens) {
        auto *model = equationPanel->findChild<QRadioButton *>(QStringLiteral("turbulenceModel_") + token);
        if (!check(model, "Missing turbulence model")) return 105;
        model->setChecked(true);
        int selectedModels = 0;
        for (auto *option : turbulenceOptions->findChildren<QRadioButton *>()) selectedModels += option->isChecked();
        aerosim::GoverningEquationsPanel restoredModel(equationDraft);
        if (!check(selectedModels == 1 && equationDraft.value("turbulenceModel") == token
                   && restoredModel.findChild<QRadioButton *>(QStringLiteral("turbulenceModel_") + token)->isChecked(),
                   "Turbulence model selection must be exclusive and restore its draft")) return 106;
    }
    laminar->setChecked(true);
    turbulent->setChecked(true);
    if (!check(equationPanel->findChild<QRadioButton *>("turbulenceModel_wilcox-kw2006")->isChecked(),
               "Hiding turbulence settings must retain the selected model")) return 107;
    turbulenceModel->setChecked(true);
    equationPanel->findChild<QRadioButton *>("esBgk")->setChecked(true);
    knudsen->setValue(0.25);
    freestreamParticles->setValue(150);
    gridParticles->setValue(4);
    equationPanel->findChild<QRadioButton *>("collisionModel_vhs")->setChecked(true);
    if (!check(!collisionModel->isChecked(), "Collision model radios must be exclusive")) return 103;
    rarefied->setChecked(false);
    if (!check(!knudsen->isVisibleTo(equationPanel) && !collisionModel->isVisibleTo(equationPanel)
               && equationDraft.value("knudsenNumber").toDouble() == 0.25
               && equationDraft.value("collisionSubmodel") == "es-bgk"
               && equationDraft.value("collisionModel") == "vhs"
               && freestreamParticles->minimum() == 1 && knudsen->minimum() > 0,
               "Rarefied switch must preserve values and hide editors")) return 99;
    rarefied->setChecked(true);
    projectTree->setCurrentItem(parameterNode->child(1));
    projectTree->setCurrentItem(parameterNode->child(0));
    if (!check(knudsen->isVisibleTo(equationPanel) && knudsen->value() == 0.25 && turbulenceModel->isVisibleTo(equationPanel),
               "Equation page navigation lost configuration")) return 100;
    {
        aerosim::GoverningEquationsPanel restored(equationDraft);
        if (!check(restored.findChild<QDoubleSpinBox *>("knudsenNumber")->value() == 0.25
                   && restored.findChild<QRadioButton *>("esBgk")->isChecked()
                   && restored.findChild<QRadioButton *>("collisionModel_vhs")->isChecked()
                   && restored.findChild<QSpinBox *>("gridParticleCount")->value() == 4,
                   "Equation controls must restore the complete draft")) return 101;
    }
    // Keep visual captures on the supplied reference's example settings.
    equationPanel->findChild<QRadioButton *>("standardBgk")->setChecked(true);
    knudsen->setValue(1.0);
    freestreamParticles->setValue(100);
    gridParticles->setValue(2);
    collisionModel->setChecked(true);
    auto *projectileItem = preprocessNode->child(1);
    projectTree->setCurrentItem(projectileItem);
    if (!check(projectileItem->text(0) == text(u"抛弹管理") && !projectileItem->icon(0).isNull()
               && workspace->currentWidget()->objectName() == "projectileManagementPage"
               && workspace->currentWidget()->findChildren<QWidget *>().isEmpty(),
               "Merged projectile navigation must retain its icon and blank page")) return 94;
    projectTree->setCurrentItem(parameterNode->child(3));
    auto *methodPage = workspace->currentWidget();
    auto *methodPanel = methodPage->findChild<aerosim::SolutionMethodPanel *>();
    if (!check(methodPanel && methodPage->objectName() == "parameterEditor_solution-method",
               "Solution method must be embedded in the parameter workspace")) return 159;
    auto *spatial = methodPanel->findChild<QComboBox *>("spatialScheme");
    auto *limiter = methodPanel->findChild<QComboBox *>("limiter");
    auto *timeScheme = methodPanel->findChild<QComboBox *>("timeScheme");
    auto *timeStep = methodPanel->findChild<QDoubleSpinBox *>("timeStep");
    auto *subIterations = methodPanel->findChild<QSpinBox *>("subIterations");
    auto *cflCount = methodPanel->findChild<QSpinBox *>("cflPointCount");
    auto *cflTable = methodPanel->findChild<QTableWidget *>("cflTable");
    auto *strongStability = methodPanel->findChild<QCheckBox *>("strongStability");
    if (!check(spatial->count() == 5 && spatial->currentData() == "hllc"
               && limiter->count() == 5 && limiter->currentData() == "vanleer"
               && timeScheme->count() == 6 && timeScheme->currentData() == "bdf1"
               && subIterations->value() == 10 && cflCount->value() == 2 && !strongStability->isChecked()
               && timeScheme->isHidden() && timeStep->isHidden() && subIterations->isHidden()
               && qobject_cast<QSpinBox *>(cflTable->cellWidget(1, 0))->value() == 2000
               && qobject_cast<QDoubleSpinBox *>(cflTable->cellWidget(1, 1))->value() == 5.,
               "Solution defaults and steady field gating must match the reference")) return 160;
    QVariantMap methodDraft;
    QObject::connect(methodPanel, &aerosim::SolutionMethodPanel::draftChanged, &window,
                     [&methodDraft](const QVariantMap &draft) { methodDraft = draft; });
    projectTree->setCurrentItem(parameterNode->child(0));
    equationPanel->findChild<QRadioButton *>("calculationType_unsteady")->setChecked(true);
    projectTree->setCurrentItem(parameterNode->child(3));
    if (!check(!timeScheme->isHidden() && !timeStep->isHidden() && !subIterations->isHidden(),
               "Unsteady equations must enable time discretization controls")) return 161;
    spatial->setCurrentIndex(3); limiter->setCurrentIndex(4); timeScheme->setCurrentIndex(5);
    timeStep->setValue(0.000125); subIterations->setValue(25); strongStability->setChecked(true);
    qobject_cast<QDoubleSpinBox *>(cflTable->cellWidget(1, 1))->setValue(7.25);
    cflCount->setValue(1); cflCount->setValue(3);
    if (!check(qobject_cast<QDoubleSpinBox *>(cflTable->cellWidget(1, 1))->value() == 7.25
               && methodDraft.value("cflPoints").toList().size() == 3
               && methodDraft.value("timeStep").toDouble() == 0.000125
               && methodDraft.value("spatialScheme") == "ausmpw-plus" && methodDraft.value("limiter") == "hvanalbada"
               && methodDraft.value("timeScheme") == "ssprk3" && methodDraft.value("strongStability").toBool(),
               "Solution changes must retain CFL rows and full precision")) return 162;
    {
        aerosim::SolutionMethodPanel restored(methodDraft);
        if (!check(restored.findChild<QSpinBox *>("cflPointCount")->value() == 3
                   && restored.findChild<QDoubleSpinBox *>("timeStep")->value() == 0.000125
                   && qobject_cast<QDoubleSpinBox *>(restored.findChild<QTableWidget *>("cflTable")->cellWidget(1, 1))->value() == 7.25,
                   "Solution method must restore its draft")) return 163;
    }
    projectTree->setCurrentItem(parameterNode->child(0)); calculationType->setChecked(true);
    projectTree->setCurrentItem(parameterNode->child(3));
    if (!check(workspace->currentWidget() == methodPage && timeStep->isHidden() && timeStep->value() == 0.000125,
               "Solution values must survive calculation type and page changes")) return 164;
    methodPage->findChild<QPushButton *>("resetParameterDraft")->click();
    methodPanel = workspace->currentWidget()->findChild<aerosim::SolutionMethodPanel *>();
    if (!check(methodPanel && methodPanel->findChild<QComboBox *>("spatialScheme")->currentData() == "hllc"
               && methodPanel->findChild<QSpinBox *>("cflPointCount")->value() == 2,
               "Clearing solution method draft must restore defaults")) return 165;
    projectTree->setCurrentItem(parameterNode->child(5));
    auto *outputPage = workspace->currentWidget();
    auto *outputPanel = outputPage->findChild<aerosim::OutputControlPanel *>();
    if (!check(outputPanel && outputPage->objectName() == "parameterEditor_output-control",
               "Output control must be embedded in the parameter workspace")) return 166;
    auto *flowList = outputPanel->findChild<QListWidget *>("flowVariablesList");
    auto *wallList = outputPanel->findChild<QListWidget *>("wallVariablesList");
    auto *historyList = outputPanel->findChild<QListWidget *>("boundaryHistoryList");
    if (!check(outputPanel->findChild<QSpinBox *>("restartInterval")->value() == 100
               && outputPanel->findChild<QSpinBox *>("wallInterval")->value() == -1
               && flowList->count() == 20 && wallList->count() == 20 && historyList->count() == 5
               && flowList->item(0)->checkState() == Qt::Checked && flowList->item(5)->checkState() == Qt::Unchecked
               && wallList->item(9)->checkState() == Qt::Checked,
               "Output defaults must match the reference")) return 167;
    for (int index = 0; index < flowList->count(); ++index) {
        if (!check(flowList->item(index)->data(Qt::UserRole) == wallList->item(index)->data(Qt::UserRole)
                   && flowList->item(index)->text() == wallList->item(index)->text(),
                   "Flow and wall output must expose the same complete variable list")) return 171;
    }
    auto *all = outputPanel->findChildren<QPushButton *>()[0];
    all->click();
    if (!check(flowList->item(5)->checkState() == Qt::Checked,
               "Output variable select-all must check every flow field")) return 168;
    QVariantMap outputDraft;
    QObject::connect(outputPanel, &aerosim::OutputControlPanel::draftChanged, &window,
                     [&outputDraft](const QVariantMap &draft) { outputDraft = draft; });
    outputPanel->findChild<QPushButton *>("resetParameterDraft");
    flowList->item(0)->setCheckState(Qt::Unchecked);
    outputPanel->findChild<QSpinBox *>("restartInterval")->setValue(250);
    if (!check(outputDraft.value("restartInterval").toInt() == 250
               && !outputDraft.value("flowVariables").toStringList().contains("pressure"),
               "Output changes must publish intervals and selected variables")) return 169;
    outputPage->findChild<QPushButton *>("resetParameterDraft")->click();
    outputPanel = workspace->currentWidget()->findChild<aerosim::OutputControlPanel *>();
    if (!check(outputPanel && outputPanel->findChild<QSpinBox *>("restartInterval")->value() == 100
               && outputPanel->findChild<QListWidget *>("flowVariablesList")->item(0)->checkState() == Qt::Checked,
               "Clearing output draft must restore defaults")) return 170;
    projectTree->setCurrentItem(parameterNode->child(6));
    auto *initialPage=workspace->currentWidget();
    auto *initialPanel=initialPage->findChild<aerosim::InitializationPanel *>();
    if(!check(initialPanel && initialPage->objectName()=="parameterEditor_initialization",
              "Initialization must be embedded")) return 172;
    QVariantMap initialDraft;
    QObject::connect(initialPanel,&aerosim::InitializationPanel::draftChanged,&window,[&](const QVariantMap &d){initialDraft=d;});
    auto *initialMode=initialPanel->findChild<QComboBox *>("initializationMode");
    auto *initialSource=initialPanel->findChild<QComboBox *>("initializationBoundary");
    auto *initialState=initialPanel->findChild<QWidget *>("globalInitializationState");
    if(!check(initialMode->currentData()=="custom" && initialSource->isHidden()
              && initialState->findChild<QDoubleSpinBox *>("machNumber")->value()==6.,
              "Initialization custom defaults must match reference")) return 173;
    initialState->findChild<QDoubleSpinBox *>("machNumber")->setValue(7.25);
    initialState->findChild<QComboBox *>("initialVelocityDefinition")->setCurrentIndex(1);
    if(!check(initialState->findChild<QDoubleSpinBox *>("angleOfAttack")->isHidden()
              && !initialState->findChild<QDoubleSpinBox *>("directionX")->isHidden(),
              "Velocity definition must switch matching fields")) return 174;
    initialPanel->setSources({{"boundaries",QVariantList{QVariantMap{{"id","test"},{"name","test"},{"type","farfield"},
        {"farfield",QVariantMap{{"machNumber",4.5},{"staticPressure",900.}}}}}}},{{"machNumber",2.5},{"parameterType","reynolds-temperature"}});
    initialMode->setCurrentIndex(1);
    initialState=initialPanel->findChild<QWidget *>("globalInitializationState");
    if(!check(!initialSource->isHidden() && !initialState->isEnabled()
              && initialState->findChild<QDoubleSpinBox *>("machNumber")->value()==4.5,
              "Boundary initialization must preview source without allowing edits")) return 175;
    initialMode->setCurrentIndex(2);
    initialState=initialPanel->findChild<QWidget *>("globalInitializationState");
    if(!check(initialSource->isHidden() && initialState->findChild<QDoubleSpinBox *>("machNumber")->value()==2.5,
              "Reference initialization must preview reference draft")) return 176;
    initialMode->setCurrentIndex(0);
    if(!check(initialPanel->findChild<QWidget *>("globalInitializationState")->findChild<QDoubleSpinBox *>("machNumber")->value()==7.25,
              "Switching source must preserve custom initialization")) return 177;
    auto *regionCount=initialPanel->findChild<QSpinBox *>("initializationRegionCount");regionCount->setValue(2);
    QTimer::singleShot(0,&window,[&]{
        auto *d=initialPanel->findChild<QDialog *>("localInitializationDialog");if(!d)return;
        d->findChild<QDoubleSpinBox *>("xMax")->setValue(3.);
        d->findChild<QDoubleSpinBox *>("machNumber")->setValue(8.);
        d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    initialPanel->findChild<QPushButton *>("initializationRegion0Settings")->click();
    regionCount->setValue(0);regionCount->setValue(2);
    if(!check(initialDraft.value("regions").toList()[0].toMap().value("xMax").toDouble()==3.,
              "Local regions must preserve inactive parameters")) return 178;
    const auto beforeCancel=initialDraft;
    QTimer::singleShot(0,&window,[&]{auto *d=initialPanel->findChild<QDialog *>("localInitializationDialog");if(d){d->findChild<QDoubleSpinBox *>("xMax")->setValue(99.);d->reject();}});
    initialPanel->findChild<QPushButton *>("initializationRegion0Settings")->click();
    if(!check(initialDraft==beforeCancel,"Cancel local initialization must discard edits")) return 179;
    auto *regionTable=initialPanel->findChild<QTableWidget *>("initializationRegions");
    auto *regionShape=qobject_cast<QComboBox *>(regionTable->cellWidget(0,0));
    auto *regionType=qobject_cast<QComboBox *>(regionTable->cellWidget(0,1));
    regionType->setCurrentIndex(1);
    bool geometryChecked=regionShape->count()==4;
    for(int shapeIndex=0;shapeIndex<4;++shapeIndex) {
        regionShape->setCurrentIndex(shapeIndex);
        QTimer::singleShot(0,&window,[&,shapeIndex]{
            auto *d=initialPanel->findChild<QDialog *>("localInitializationDialog");if(!d)return;
            geometryChecked=geometryChecked && d->findChildren<QComboBox *>().isEmpty()
                && d->findChild<QDoubleSpinBox *>("unitReynoldsNumber")
                && !d->findChild<QDoubleSpinBox *>("unitReynoldsNumber")->isHidden()
                && d->findChild<QDoubleSpinBox *>("machNumber")->value()==8.;
            if(!motionCaptures.isEmpty()) {app.processEvents();d->grab().save(QDir(motionCaptures).filePath(QStringLiteral("initial-region-%1.png").arg(shapeIndex)));}
            if(shapeIndex==1 || shapeIndex==2)d->findChild<QDoubleSpinBox *>("radius")->setValue(2.);
            if(shapeIndex>=2)d->findChild<QDoubleSpinBox *>("axis2Z")->setValue(4.);
            if(shapeIndex==3)d->findChild<QDoubleSpinBox *>("radius1")->setValue(3.);
            d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        initialPanel->findChild<QPushButton *>("initializationRegion0Settings")->click();
    }
    regionShape->setCurrentIndex(1);
    QTimer::singleShot(0,&window,[&]{auto *d=initialPanel->findChild<QDialog *>("localInitializationDialog");if(!d)return;
        geometryChecked=geometryChecked && d->findChild<QDoubleSpinBox *>("radius")->value()==2.;d->reject();});
    initialPanel->findChild<QPushButton *>("initializationRegion0Settings")->click();
    if(!check(geometryChecked,"Local shapes must expose fixed parameters and retain geometry values")) return 181;
    initialPage->findChild<QPushButton *>("resetParameterDraft")->click();
    if(!check(workspace->currentWidget()->findChild<QSpinBox *>("initializationRegionCount")->value()==0,
              "Reset initialization must restore defaults")) return 180;
    projectTree->setCurrentItem(parameterNode->child(4));
    auto *referencePanel = window.findChild<aerosim::ReferenceValuesPanel *>();
    if (!check(referencePanel && workspace->currentWidget()->objectName() == "parameterEditor_reference-values",
               "Reference settings must be embedded in the main workspace")) return 108;
    auto *referenceMode = referencePanel->findChild<QComboBox *>("referenceInputMode");
    auto *referenceType = referencePanel->findChild<QComboBox *>("referenceParameterType");
    auto *referenceSource = referencePanel->findChild<QWidget *>("referenceBoundarySource");
    const auto referenceNumber = [referencePanel](const QString &key) {
        return referencePanel->findChild<QDoubleSpinBox *>(key);
    };
    if (!check(referenceMode->currentData() == "boundary" && !referenceSource->isHidden()
               && referenceType->count() == 4 && referenceNumber("machNumber")->value() == 3.0
               && referenceNumber("staticTemperature")->value() == 60.0
               && referenceNumber("referenceArea")->value() == 1.0,
               "Reference settings defaults must match supplied examples")) return 109;
    QVariantMap referenceDraft;
    QObject::connect(referencePanel, &aerosim::ReferenceValuesPanel::draftChanged, &window,
                     [&referenceDraft](const QVariantMap &draft) { referenceDraft = draft; });
    auto *sourceForm = qobject_cast<QFormLayout *>(referenceSource->parentWidget()->layout());
    auto *sourceLabel = sourceForm->labelForField(referenceSource);
    referenceMode->setCurrentIndex(1);
    if (!check(referenceSource->isHidden() && sourceLabel->isHidden()
               && referenceDraft.value("inputMode") == "custom",
               "Custom input must hide both boundary selector and label")) return 110;
    referenceNumber("staticTemperature")->setValue(285.0);
    const QStringList conditionalKeys = {"unitReynoldsNumber", "staticTemperature", "staticPressure",
                                         "totalTemperature", "totalPressure", "altitude"};
    const QList<QStringList> visibleFields = {{"unitReynoldsNumber", "staticTemperature"},
        {"staticTemperature", "staticPressure"}, {"totalTemperature", "totalPressure"}, {"altitude"}};
    for (int type = 0; type < 4; ++type) {
        referenceType->setCurrentIndex(type);
        for (const auto &key : conditionalKeys) {
            auto *editor = referenceNumber(key);
            const bool active = visibleFields[type].contains(key);
            if (!check(editor->isHidden() != active
                       && (active ? !sourceForm->labelForField(editor)->isHidden()
                                  : sourceForm->indexOf(editor) == -1),
                       "Reference type must show exactly its matching fields and labels")) return 111;
        }
    }
    referenceNumber("altitude")->setValue(12000.0);
    referenceType->setCurrentIndex(0);
    referenceMode->setCurrentIndex(0);
    if (!check(!referenceSource->isHidden() && !sourceLabel->isHidden()
               && referenceNumber("staticTemperature")->value() == 285.0
               && referenceDraft.value("altitude").toDouble() == 12000.0,
               "Reference mode/type changes must retain entered values")) return 112;
    projectTree->setCurrentItem(parameterNode->child(0));
    projectTree->setCurrentItem(parameterNode->child(4));
    if (!check(referenceNumber("staticTemperature")->value() == 285.0,
               "Reference navigation must retain edits")) return 113;
    {
        aerosim::ReferenceValuesPanel restoredReference(referenceDraft);
        if (!check(restoredReference.findChild<QDoubleSpinBox *>("staticTemperature")->value() == 285.0
                   && restoredReference.findChild<QDoubleSpinBox *>("altitude")->value() == 12000.0,
                   "Reference panel must restore all fields including hidden values")) return 114;
    }
    workspace->currentWidget()->findChild<QPushButton *>("resetParameterDraft")->click();
    referencePanel = workspace->currentWidget()->findChild<aerosim::ReferenceValuesPanel *>();
    if (!check(referencePanel && referencePanel->findChild<QDoubleSpinBox *>("staticTemperature")->value() == 60.0
               && referencePanel->findChild<QComboBox *>("referenceInputMode")->currentIndex() == 0,
               "Clearing reference draft must restore defaults")) return 115;
    referenceMode = referencePanel->findChild<QComboBox *>("referenceInputMode");
    referenceType = referencePanel->findChild<QComboBox *>("referenceParameterType");
    projectTree->setCurrentItem(parameterNode->child(7));
    auto *calculationPage = workspace->currentWidget();
    auto *controlPanel = calculationPage->findChild<aerosim::CalculationControlPanel *>();
    if (!check(controlPanel && calculationPage->objectName() == "parameterEditor_calculation-control"
               && window.findChild<QDockWidget *>("propertyDock")->isHidden(),
               "Calculation control must be embedded in the parameter workspace")) return 116;
    auto *firstOrder = controlPanel->findChild<QSpinBox *>("firstOrderIterations");
    auto *maximumIterations = controlPanel->findChild<QSpinBox *>("maximumIterations");
    auto *convergenceResidual = controlPanel->findChild<QDoubleSpinBox *>("convergenceResidual");
    auto *monitorThreshold = controlPanel->findChild<QDoubleSpinBox *>("residualMonitorThreshold");
    auto *useGpu = controlPanel->findChild<QComboBox *>("useGpu");
    auto *processCount = controlPanel->findChild<QSpinBox *>("processCount");
    if (!check(firstOrder->value() == 2000 && maximumIterations->value() == 30000
               && convergenceResidual->value() == -10.0 && monitorThreshold->value() == 100.0
               && !useGpu->currentData().toBool() && useGpu->count() == 2 && processCount->value() == 4
               && firstOrder->minimum() == 0 && maximumIterations->minimum() == 1 && processCount->minimum() == 1,
               "Calculation control defaults must match the supplied reference")) return 117;
    QVariantMap calculationDraft;
    QObject::connect(controlPanel, &aerosim::CalculationControlPanel::draftChanged, &window,
                     [&calculationDraft](const QVariantMap &draft) { calculationDraft = draft; });
    firstOrder->setValue(1500);
    maximumIterations->setValue(40000);
    convergenceResidual->setValue(-8.5);
    monitorThreshold->setValue(75.25);
    useGpu->setCurrentIndex(1);
    processCount->setValue(8);
    if (!check(calculationDraft.value("firstOrderIterations") == 1500
               && calculationDraft.value("maximumIterations") == 40000
               && calculationDraft.value("convergenceResidual").toDouble() == -8.5
               && calculationDraft.value("residualMonitorThreshold").toDouble() == 75.25
               && calculationDraft.value("useGpu").toBool() && calculationDraft.value("processCount") == 8,
               "Calculation changes must publish a complete draft without losing residual precision")) return 118;
    projectTree->setCurrentItem(parameterNode->child(4));
    projectTree->setCurrentItem(parameterNode->child(7));
    if (!check(workspace->currentWidget() == calculationPage && firstOrder->value() == 1500
               && processCount->value() == 8 && useGpu->currentData().toBool(),
               "Calculation settings must survive page navigation")) return 119;
    {
        aerosim::CalculationControlPanel restoredControl(calculationDraft);
        if (!check(restoredControl.findChild<QSpinBox *>("maximumIterations")->value() == 40000
                   && restoredControl.findChild<QDoubleSpinBox *>("convergenceResidual")->value() == -8.5
                   && restoredControl.findChild<QDoubleSpinBox *>("residualMonitorThreshold")->value() == 75.25
                   && restoredControl.findChild<QComboBox *>("useGpu")->currentData().toBool()
                   && restoredControl.findChild<QSpinBox *>("processCount")->value() == 8,
                   "Calculation control must restore its draft")) return 120;
    }
    calculationPage->findChild<QPushButton *>("resetParameterDraft")->click();
    controlPanel = workspace->currentWidget()->findChild<aerosim::CalculationControlPanel *>();
    if (!check(controlPanel && controlPanel->findChild<QSpinBox *>("firstOrderIterations")->value() == 2000
               && controlPanel->findChild<QSpinBox *>("maximumIterations")->value() == 30000
               && controlPanel->findChild<QDoubleSpinBox *>("convergenceResidual")->value() == -10.0
               && !controlPanel->findChild<QComboBox *>("useGpu")->currentData().toBool()
               && controlPanel->findChild<QSpinBox *>("processCount")->value() == 4,
               "Clearing calculation draft must restore defaults")) return 121;
    const QString parameterCaptures = qEnvironmentVariable("AEROSIM_PARAMETER_SCREENSHOTS");
    const auto settleParameterCapture = [&app, &window] {
        // Allow native Windows layout and paint events to settle before grabbing.
        QEventLoop events;
        QTimer::singleShot(120, &events, &QEventLoop::quit);
        events.exec();
        window.repaint();
        app.processEvents();
    };
    if (!parameterCaptures.isEmpty()) {
        QDir().mkpath(parameterCaptures);
        projectTree->setCurrentItem(parameterNode->child(0));
        window.resize(1440, 960);
        window.show();
        laminar->setChecked(true);
        rarefied->setChecked(false);
        settleParameterCapture();
        window.grab().save(QDir(parameterCaptures).filePath("equations-collapsed.png"));
        turbulent->setChecked(true);
        rarefied->setChecked(true);
    }

    if (!parameterCaptures.isEmpty()) {
        QDir().mkpath(parameterCaptures);
        window.resize(1440, 960);
        window.show();
        for (int index = 0; index < parameterNode->childCount(); ++index) {
            projectTree->setCurrentItem(parameterNode->child(index));
            settleParameterCapture();
            window.grab().save(QDir(parameterCaptures).filePath(QStringLiteral("parameter-%1.png").arg(index)));
        }
        window.resize(1000, 720);
        for (int index = 0; index < 4; ++index) {
            projectTree->setCurrentItem(parameterNode->child(index));
            settleParameterCapture();
            window.grab().save(QDir(parameterCaptures).filePath(QStringLiteral("narrow-%1.png").arg(index)));
        }
        projectTree->setCurrentItem(parameterNode->child(4));
        settleParameterCapture();
        window.grab().save(QDir(parameterCaptures).filePath("reference-narrow.png"));
        projectTree->setCurrentItem(parameterNode->child(7));
        settleParameterCapture();
        window.grab().save(QDir(parameterCaptures).filePath("calculation-narrow.png"));
        projectTree->setCurrentItem(parameterNode->child(1));
        settleParameterCapture();
        auto *boundaryScroll = workspace->currentWidget()->findChild<QScrollArea *>("parameterScroll");
        boundaryScroll->ensureWidgetVisible(farfieldParameters);
        settleParameterCapture();
        window.grab().save(QDir(parameterCaptures).filePath("farfield-narrow-editor.png"));
        window.resize(1440, 960);
        projectTree->setCurrentItem(parameterNode->child(1));
        for (int type = 0; type < 4; ++type) {
            farfieldType->setCurrentIndex(type);
            settleParameterCapture();
            window.grab().save(QDir(parameterCaptures).filePath(QStringLiteral("farfield-%1.png").arg(type)));
        }
        farfieldType->setCurrentIndex(1);
        boundaryTable->setCurrentItem(editedBoundary);
        for (const QString &boundary : {QStringLiteral("no-slip-isothermal-wall"),
                                        QStringLiteral("mass-flow-subsonic-outlet"),
                                        QStringLiteral("mass-flow-subsonic-inlet")}) {
            boundaryType->setCurrentIndex(boundaryType->findData(boundary));
            if (boundary == "no-slip-isothermal-wall") wallTemperature->setValue(300.0);
            else massFlow->setValue(100.0);
            if (boundary == "mass-flow-subsonic-inlet") inletTemperature->setValue(300.0);
            settleParameterCapture();
            window.grab().save(QDir(parameterCaptures).filePath(boundary + QStringLiteral(".png")));
        }
        for (int index = 0; index < pressureTypes.size(); ++index) {
            boundaryType->setCurrentIndex(boundaryType->findData(pressureTypes[index]));
            for (const auto &key : pressureKeys[index])
                boundaryPanel->findChild<QDoubleSpinBox *>(QStringLiteral("boundary_") + key)
                    ->setValue(pressureDefaults.value(key).toDouble());
            settleParameterCapture();
            window.grab().save(QDir(parameterCaptures).filePath(pressureTypes[index] + QStringLiteral(".png")));
        }
        boundaryTable->setCurrentItem(farfieldBoundary);
        projectTree->setCurrentItem(parameterNode->child(4));
        referenceMode->setCurrentIndex(1);
        for (int type = 0; type < 4; ++type) {
            referenceType->setCurrentIndex(type);
            settleParameterCapture();
            window.grab().save(QDir(parameterCaptures).filePath(QStringLiteral("reference-custom-%1.png").arg(type)));
        }

    }
    QTextStream(stdout) << "UI ownership, service binding, workflow gating and task display passed\n";
    return 0;
}
