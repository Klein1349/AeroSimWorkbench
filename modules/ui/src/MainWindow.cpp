#include "aerosim/ui/MainWindow.h"

#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N>
QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}

QString taskStatus(TaskStatus status)
{
    switch (status) {
    case TaskStatus::Preparing: return uiText(u"准备中");
    case TaskStatus::Running: return uiText(u"运行中");
    case TaskStatus::Paused: return uiText(u"已暂停");
    case TaskStatus::Finished: return uiText(u"已完成");
    case TaskStatus::Failed: return uiText(u"失败");
    case TaskStatus::Cancelled: return uiText(u"已取消");
    default: return uiText(u"未知");
    }
}
QString stepStatus(StepState state)
{
    switch (state) {
    case StepState::Pending: return uiText(u"待配置");
    case StepState::NeedsValidation: return uiText(u"待校验");
    case StepState::Complete: return uiText(u"已完成");
    case StepState::Blocked: return uiText(u"被阻塞");
    }
    return {};
}
QLabel *emptyState(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setMargin(20);
    return label;
}
}

MainWindow::MainWindow(QWidget *parent) : SARibbonMainWindow(parent)
{
    setObjectName(uiText(u"AeroSimMainWindow"));
    setWindowTitle(uiText(u"AeroSimWorkbench · 仿真工作台（Mock 服务）"));
    QFont workbenchFont(uiText(u"Microsoft YaHei UI"));
    workbenchFont.setPointSize(9);
    setFont(workbenchFont);
    ribbonBar()->setRibbonStyle(SARibbonBar::RibbonStyleCompactTwoRow);
    resize(1440, 960);
    setMinimumSize(1000, 720);
    setDockNestingEnabled(true);
    createWorkspace();
    createRibbon();
    status_ = new QLabel(uiText(u"就绪  |  工程：未打开  |  网格：未载入  |  求解器：未接入  |  MPI：—"), this);
    statusBar()->addWidget(status_, 1);
    statusBar()->addPermanentWidget(new QLabel(uiText(u"UI 框架 · Mock 服务"), this));
    QTimer::singleShot(0, this, [this] {
        setStyleSheet(styleSheet() + uiText(u"QDockWidget::title { background: #e9eef5; padding: 7px; color: #25364c; }"
            "QTreeWidget { border: 0; alternate-background-color: #f4f7fb; }"
            "QTreeWidget::item { padding: 5px 2px; }"
            "QTreeWidget::item:selected { background: #dceafd; color: #153b6b; }"
            "QTabWidget::pane { border: 1px solid #dce2eb; }"
            "QStatusBar { background: #e9eef5; color: #25364c; }"
            "QWidget#viewportSlot { background: #edf2f8; color: #52677f; }"
            "QLabel#propertyTitle { font-size: 16px; font-weight: 600; color: #234d7a; }"
            "QPushButton { padding: 6px 12px; }"));
    });
    restoreLayout();
    appendMessage(uiText(u"主界面已就绪。工程树与参数为界面示例；尚未接入工程、配置、VTK 和真实求解服务。"));
}

void MainWindow::createRibbon()
{
    struct Group { const char *title; const char *items; };
    const Group groups[] = {
        {"工程", "新建工程|打开工程|保存工程|导入配置|导出配置"},
        {"前处理", "导入网格|网格变换|大气参数|边界条件|初始化"},
        {"状态管理", "新建状态|状态模板|多状态配置"},
        {"求解", "求解器设置|并行设置|多状态计算 / 传热|耦合控制|启动|暂停|续算|停止"},
        {"监控", "计算状态|日志|收敛曲线|任务管理"},
        {"后处理", "后处理工作台|云图|切面|等值面|流线|曲线|动画"},
        {"数据处理", "多状态数据处理"},
        {"工具", "恢复默认布局|工程导航|属性面板|输出面板"},
        {"帮助", "使用说明|关于"}
    };
    for (const auto &group : groups) {
        const QString title = QString::fromUtf8(group.title);
        auto *category = ribbonBar()->addCategoryPage(title);
        auto *panel = category->addPanel(title + uiText(u"操作"));
        for (const QString &name : QString::fromUtf8(group.items).split('|')) {
            QStyle::StandardPixmap icon = QStyle::SP_FileDialogDetailedView;
            if (name.contains(uiText(u"新建"))) icon = QStyle::SP_FileIcon;
            else if (name.contains(uiText(u"打开")) || name.contains(uiText(u"导入"))) icon = QStyle::SP_DialogOpenButton;
            else if (name.contains(uiText(u"保存")) || name.contains(uiText(u"导出"))) icon = QStyle::SP_DialogSaveButton;
            else if (name == uiText(u"启动") || name == uiText(u"续算")) icon = QStyle::SP_MediaPlay;
            else if (name == uiText(u"暂停")) icon = QStyle::SP_MediaPause;
            else if (name == uiText(u"停止")) icon = QStyle::SP_MediaStop;
            auto *action = new QAction(style()->standardIcon(icon), name, this);
            panel->addLargeAction(action);
            action->setEnabled(false);
            action->setToolTip(name + uiText(u"：等待对应业务模块接入"));
            if (name == uiText(u"后处理工作台")) {
                action->setObjectName(uiText(u"openPostProcessAction"));
                action->setEnabled(true);
                action->setToolTip(uiText(u"打开数据、场景、时间轴与可选 VTK 视口"));
                connect(action, &QAction::triggered, this, &MainWindow::openPostProcessRequested);
            } else if (name == uiText(u"启动")) {
                runAction_ = action;
                action->setObjectName(uiText(u"runAction"));
                connect(action, &QAction::triggered, this, &MainWindow::submitRun);
            } else if (title == uiText(u"监控")) {
                const int index = name == uiText(u"日志") ? 0 : name == uiText(u"计算状态") ? 1 : name == uiText(u"收敛曲线") ? 2 : 4;
                action->setEnabled(true);
                action->setToolTip(uiText(u"打开") + name);
                connect(action, &QAction::triggered, this, [this, index] { showOutput(index); });
            } else if (title == uiText(u"工具")) {
                action->setEnabled(true);
                action->setToolTip(name);
                connect(action, &QAction::triggered, this, [this, name] {
                    if (name == uiText(u"恢复默认布局")) restoreLayout();
                    else if (name == uiText(u"工程导航")) projectDock_->show();
                    else if (name == uiText(u"属性面板")) propertyDock_->show();
                    else outputDock_->show();
                });
            } else if (title == uiText(u"帮助")) {
                action->setEnabled(true);
                action->setToolTip(name);
                connect(action, &QAction::triggered, this, [this] {
                    appendMessage(uiText(u"使用说明：左侧选择示例对象，右侧编辑本地草稿；拖动面板可调整布局。工作流状态由服务提供。灰色功能等待业务模块接入，Mock 任务不执行真实求解。"));
                    showOutput(0);
                });
            } else {
                const QHash<QString, QString> pages = {
                    {uiText(u"边界条件"), "boundary"}, {uiText(u"大气参数"), "physics"},
                    {uiText(u"求解器设置"), "solver"}, {uiText(u"并行设置"), "parallel"},
                    {uiText(u"多状态配置"), "states"}, {uiText(u"网格变换"), "mesh"}};
                if (pages.contains(name)) {
                    action->setEnabled(true);
                    action->setToolTip(uiText(u"打开参数草稿（尚未持久化）"));
                    connect(action, &QAction::triggered, this, [this, pages, name] { selectPage(pages.value(name)); });
                }
            }
        }
    }
}

QWidget *MainWindow::createSlot(WidgetSlot slot, const QString &text)
{
    auto *host = new QWidget(this);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(emptyState(text, host));
    slotLayouts_.insert(static_cast<int>(slot), layout);
    return host;
}

void MainWindow::createWorkspace()
{
    auto *central = new QWidget(this);
    auto *centerLayout = new QVBoxLayout(central);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    auto *viewToolbar = new QToolBar(uiText(u"视图控制"), central);
    for (const QString &name : uiText(u"适应|放大|缩小|旋转|正视 X|侧视 Y|顶视 Z|表面|线框").split('|')) {
        auto *action = viewToolbar->addAction(name);
        action->setEnabled(false);
        action->setToolTip(uiText(u"等待 VTK 视口及视图控制接口接入"));
    }
    centerLayout->addWidget(viewToolbar);
    auto *viewport = createSlot(WidgetSlot::Viewport, uiText(u"三维工作区\n\nVTK 8.2 视口接入位置\n导入与渲染模块尚未接入"));
    viewport->setObjectName(uiText(u"viewportSlot"));
    centerLayout->addWidget(viewport, 1);
    setCentralWidget(central);

    projectDock_ = new QDockWidget(uiText(u"工程与工作流"), this);
    projectDock_->setObjectName(uiText(u"projectDock"));
    navigationTabs_ = new QTabWidget(projectDock_);
    projectTree_ = new QTreeWidget(navigationTabs_);
    projectTree_->setObjectName(uiText(u"projectTree"));
    projectTree_->setHeaderHidden(true);
    auto *root = new QTreeWidgetItem(projectTree_, {uiText(u"界面示例（未打开工程）")});
    const QList<QStringList> nodes = {
        {"mesh", "网格 Mesh", "inlet|outlet|wall"},
        {"physics", "物理模型 Physics", "气体模型|流动模型|湍流模型"},
        {"motion", "运动 Motion", "动网格|强迫运动|六自由度"},
        {"boundary", "边界条件", "inlet|outlet|wall"},
        {"solver", "求解器 Solver", "空间离散|时间离散|迭代控制"},
        {"parallel", "并行计算", ""}, {"states", "状态 States", "状态配置示例"},
        {"results", "结果 Results", "压力|温度|速度|切面|流线"}
    };
    for (const auto &node : nodes) {
        auto *item = new QTreeWidgetItem(root, {node[1]});
        item->setData(0, Qt::UserRole, node[0]);
        for (const QString &child : node[2].split('|', QString::SkipEmptyParts)) {
            auto *leaf = new QTreeWidgetItem(item, {child});
            leaf->setData(0, Qt::UserRole, node[0]);
            leaf->setData(0, Qt::UserRole + 1, child);
        }
    }
    root->setExpanded(true);
    connect(projectTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (!item) return;
        const QString page = item->data(0, Qt::UserRole).toString();
        if (page.isEmpty()) return;
        selectPage(page);
        emit navigationRequested(page, item->data(0, Qt::UserRole + 1).toString(), {});
    });
    navigationTabs_->addTab(projectTree_, uiText(u"工程树"));
    auto *workflowPage = new QWidget(navigationTabs_);
    auto *workflowLayout = new QVBoxLayout(workflowPage);
    suggestion_ = new QLabel(uiText(u"等待工作流服务"), workflowPage);
    suggestion_->setWordWrap(true);
    workflowLayout->addWidget(suggestion_);
    workflowTree_ = new QTreeWidget(workflowPage);
    workflowTree_->setObjectName(uiText(u"workflowTree"));
    workflowTree_->setHeaderLabels({uiText(u"步骤 / 操作"), uiText(u"状态")});
    workflowTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    workflowLayout->addWidget(workflowTree_);
    connect(workflowTree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString page = item->data(0, Qt::UserRole).toString();
        if (!page.isEmpty()) selectPage(page);
        emit navigationRequested(page, item->data(0, Qt::UserRole + 1).toString(), item->data(0, Qt::UserRole + 2).toString());
    });
    navigationTabs_->addTab(workflowPage, uiText(u"工作流"));
    projectDock_->setWidget(navigationTabs_);

    propertyDock_ = new QDockWidget(uiText(u"属性编辑器"), this);
    propertyDock_->setObjectName(uiText(u"propertyDock"));
    auto *properties = new QWidget(propertyDock_);
    auto *propertyLayout = new QVBoxLayout(properties);
    propertyTitle_ = new QLabel(properties);
    propertyTitle_->setObjectName(uiText(u"propertyTitle"));
    propertyLayout->addWidget(propertyTitle_);
    auto *notice = new QLabel(uiText(u"参数草稿示例 · 仅保存在当前窗口\n正式参数校验与工程保存待接入"), properties);
    notice->setWordWrap(true);
    propertyLayout->addWidget(notice);
    propertyForm_ = new QFormLayout;
    propertyForm_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    propertyLayout->addLayout(propertyForm_);
    propertyLayout->addStretch();
    auto *reset = new QPushButton(uiText(u"清除本页草稿"), properties);
    connect(reset, &QPushButton::clicked, this, [this] { drafts_.remove(currentPage_); selectPage(currentPage_); });
    propertyLayout->addWidget(reset);
    propertyDock_->setWidget(properties);
    selectPage(uiText(u"mesh"));

    outputDock_ = new QDockWidget(uiText(u"输出与计算监控"), this);
    outputDock_->setObjectName(uiText(u"outputDock"));
    outputTabs_ = new QTabWidget(outputDock_);
    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    auto *logSlot = createSlot(WidgetSlot::Log, {});
    auto *logLayout = slotLayouts_.value(static_cast<int>(WidgetSlot::Log));
    auto *oldLogItem = logLayout->takeAt(0);
    delete oldLogItem->widget();
    delete oldLogItem;
    logLayout->addWidget(log_);
    outputTabs_->addTab(logSlot, uiText(u"日志"));
    auto *monitorSlot = createSlot(WidgetSlot::Monitor, {});
    auto *monitorLayout = slotLayouts_.value(static_cast<int>(WidgetSlot::Monitor));
    auto *oldMonitorItem = monitorLayout->takeAt(0);
    delete oldMonitorItem->widget();
    delete oldMonitorItem;
    auto *monitor = new QWidget(monitorSlot);
    auto *monitorForm = new QFormLayout(monitor);
    monitorStatus_ = new QLabel(uiText(u"尚无任务事件"), monitor);
    monitorDetails_ = new QLabel(uiText(u"迭代：—    残差：—    CPU：—    内存：—    MPI：—"), monitor);
    progress_ = new QProgressBar(monitor);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    monitorForm->addRow(uiText(u"任务状态"), monitorStatus_);
    monitorForm->addRow(uiText(u"计算指标"), monitorDetails_);
    monitorForm->addRow(uiText(u"进度"), progress_);
    monitorLayout->addWidget(monitor);
    outputTabs_->addTab(monitorSlot, uiText(u"计算监控"));
    outputTabs_->addTab(emptyState(uiText(u"收敛曲线\n等待残差采样数据与曲线组件接入"), outputTabs_), uiText(u"收敛曲线"));
    auto *errorSlot = createSlot(WidgetSlot::Error, {});
    auto *errorLayout = slotLayouts_.value(static_cast<int>(WidgetSlot::Error));
    auto *oldErrorItem = errorLayout->takeAt(0);
    delete oldErrorItem->widget();
    delete oldErrorItem;
    errors_ = new QPlainTextEdit(this);
    errors_->setReadOnly(true);
    errors_->setMaximumBlockCount(1000);
    errorLayout->addWidget(errors_);
    outputTabs_->addTab(errorSlot, uiText(u"错误信息"));
    taskTree_ = new QTreeWidget(outputTabs_);
    taskTree_->setObjectName(uiText(u"taskTree"));
    taskTree_->setHeaderLabels({uiText(u"任务 ID"), uiText(u"状态"), uiText(u"消息"), uiText(u"时间")});
    taskTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    taskTree_->setAlternatingRowColors(true);
    outputTabs_->addTab(taskTree_, uiText(u"任务列表"));
    outputDock_->setWidget(outputTabs_);
}

void MainWindow::selectPage(const QString &pageId)
{
    currentPage_ = pageId;
    while (propertyForm_->rowCount()) propertyForm_->removeRow(0);
    const QHash<QString, QString> titles = {{"mesh", "网格与变换"}, {"physics", "物理模型 / 大气参数"},
        {"motion", "运动参数"}, {"boundary", "边界条件"}, {"solver", "求解器设置"},
        {"parallel", "并行设置"}, {"states", "工况状态"}, {"results", "结果与后处理"}};
    propertyTitle_->setText(titles.value(pageId, pageId));
    const auto addNumber = [this, pageId](const QString &key, const QString &label, double value, const QString &unit) {
        auto *editor = new QDoubleSpinBox(this);
        editor->setObjectName(key);
        editor->setDecimals(4);
        editor->setRange(-1e9, 1e9);
        editor->setSuffix(unit);
        editor->setValue(drafts_.value(pageId).value(key, value).toDouble());
        propertyForm_->addRow(label, editor);
        connect(editor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, pageId, key](double value) {
            drafts_[pageId].insert(key, value);
            emit parameterDraftChanged(pageId, drafts_.value(pageId));
        });
    };
    if (pageId == "boundary" || pageId == "physics") {
        auto *type = new QComboBox(this);
        type->addItems({uiText(u"压力入口"), uiText(u"速度入口"), uiText(u"压力出口"), uiText(u"壁面")});
        type->setCurrentIndex(drafts_.value(pageId).value("type", 0).toInt());
        propertyForm_->addRow(uiText(u"边界类型（示例）"), type);
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, pageId](int value) {
            drafts_[pageId].insert("type", value);
            emit parameterDraftChanged(pageId, drafts_.value(pageId));
        });
        addNumber("pressure", uiText(u"压力"), 101325, " Pa");
        addNumber("temperature", uiText(u"温度"), 300, " K");
    } else if (pageId == "motion" || pageId == "mesh") {
        if (pageId == "motion") addNumber("mass", uiText(u"质量"), 15.2, " kg");
        for (const QString &axis : {uiText(u"X"), uiText(u"Y"), uiText(u"Z")})
            addNumber(axis, uiText(u"位置 ") + axis, 0, " m");
    } else if (pageId == "solver") {
        addNumber("iterations", uiText(u"最大迭代步"), 10000, {});
        addNumber("timeStep", uiText(u"时间步长"), 0.001, " s");
    } else if (pageId == "parallel") {
        addNumber("processes", uiText(u"MPI 进程数"), 32, {});
    } else {
        auto *note = new QLabel(pageId == "results" ? uiText(u"尚无结果数据。结果匹配校验和显示由工作流及可视化模块提供。") : uiText(u"选择配置对象后由专业模块提供参数。"), this);
        note->setWordWrap(true);
        propertyForm_->addRow(note);
    }
    propertyDock_->show();
}

void MainWindow::restoreLayout()
{
    for (auto *dock : {projectDock_, propertyDock_, outputDock_}) {
        dock->setFloating(false);
        removeDockWidget(dock);
    }
    addDockWidget(Qt::LeftDockWidgetArea, projectDock_);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock_);
    addDockWidget(Qt::BottomDockWidgetArea, outputDock_);
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    for (auto *dock : {projectDock_, propertyDock_, outputDock_}) dock->show();
    resizeDocks({projectDock_, propertyDock_}, {250, 285}, Qt::Horizontal);
    resizeDocks({outputDock_}, {210}, Qt::Vertical);
}

bool MainWindow::insertWidget(WidgetSlot slot, QWidget *widget)
{
    const int key = static_cast<int>(slot);
    if (QThread::currentThread() != thread() || !widget || widget == this || widget->isAncestorOf(this)
        || widget->thread() != thread() || !slotLayouts_.contains(key) || insertedWidgets_.value(key)) return false;
    // Do not accept a widget already owned by this window (including another slot).
    if (isAncestorOf(widget)) return false;
    auto *layout = slotLayouts_.value(key);
    while (auto *item = layout->takeAt(0)) {
        // Keep built-in displays alive for queued events, hidden behind the injected widget.
        item->widget()->hide();
        delete item;
    }
    layout->addWidget(widget);
    insertedWidgets_.insert(key, widget);
    return true;
}

void MainWindow::bindServices(const ServiceBindings &services)
{
    for (const auto &connection : serviceConnections_) disconnect(connection);
    serviceConnections_.clear();
    workflowView_ = {};
    workflow_ = services.workflow;
    tasks_ = services.tasks;
    registerContractMetaTypes();
    if (workflow_) {
        serviceConnections_.append(connect(workflow_, &IWorkflowService::workflowViewChanged, this,
            &MainWindow::displayWorkflow, Qt::QueuedConnection));
        // view()/requestRun() are synchronous contracts; do not call across threads.
        serviceConnections_.append(connect(workflow_, &QObject::destroyed, this, [this] {
            runAction_->setEnabled(false);
            suggestion_->setText(uiText(u"工作流服务已断开"));
        }));
    } else {
        workflowTree_->clear();
        suggestion_->setText(uiText(u"等待工作流服务"));
    }
    if (tasks_) serviceConnections_.append(connect(tasks_, &ITaskService::taskEventOccurred, this,
        &MainWindow::displayTaskEvent, Qt::QueuedConnection));
    if (runAction_) runAction_->setEnabled(false);
    if (workflow_ && workflow_->thread() == thread()) displayWorkflow(workflow_->view(runRequest_.scope));
}

void MainWindow::setRunContext(const RunRequest &request)
{
    workflowView_ = {};
    runRequest_ = request;
    hasRunContext_ = !request.inputSnapshot.snapshotId.isEmpty();
    if (workflow_ && workflow_->thread() == thread()) displayWorkflow(workflow_->view(request.scope));
}

void MainWindow::displayWorkflow(const WorkflowView &view)
{
    if (hasRunContext_ && (view.scope.projectId != runRequest_.scope.projectId || view.scope.caseId != runRequest_.scope.caseId)) return;
    if (view.scope.projectId == workflowView_.scope.projectId && view.scope.caseId == workflowView_.scope.caseId
        && view.viewRevision < workflowView_.viewRevision) return;
    workflowView_ = view;
    workflowTree_->clear();
    suggestion_->setText(view.nextSuggestion);
    if (runAction_) runAction_->setEnabled(false);
    for (const auto &step : view.steps) {
        auto *item = new QTreeWidgetItem(workflowTree_, {step.summary, stepStatus(step.state)});
        item->setData(0, Qt::UserRole, step.targetPageId);
    }
    for (const auto &action : view.actions) {
        auto *item = new QTreeWidgetItem(workflowTree_, {action.actionId, action.enabled ? uiText(u"可用") : uiText(u"不可用")});
        item->setToolTip(0, action.reason);
        item->setData(0, Qt::UserRole, action.targetPageId);
        item->setData(0, Qt::UserRole + 1, action.objectId);
        item->setData(0, Qt::UserRole + 2, action.fieldKey);
        if (action.actionId == "run" && runAction_) {
            runAction_->setEnabled(action.enabled && hasRunContext_ && workflow_ && workflow_->thread() == thread());
            runAction_->setToolTip(action.reason.isEmpty() ? uiText(u"向工作流服务请求运行") : action.reason);
        }
    }
    for (const auto &issue : view.issues) {
        auto *item = new QTreeWidgetItem(workflowTree_, {issue.message, uiText(u"校验提示")});
        item->setData(0, Qt::UserRole, issue.targetPageId);
        item->setData(0, Qt::UserRole + 1, issue.objectId);
        item->setData(0, Qt::UserRole + 2, issue.fieldKey);
    }
}

void MainWindow::submitRun()
{
    if (!workflow_ || workflow_->thread() != thread() || !hasRunContext_) return;
    const auto result = workflow_->requestRun(runRequest_);
    appendMessage(result.allowed ? uiText(u"运行请求已提交，等待任务事件") : uiText(u"工作流拒绝运行请求"));
    for (const auto &issue : result.issues) errors_->appendPlainText(issue.message);
    if (!result.allowed) showOutput(3);
}

void MainWindow::displayTaskEvent(const TaskEvent &event)
{
    QTreeWidgetItem *row = nullptr;
    for (int i = 0; i < taskTree_->topLevelItemCount(); ++i) {
        if (taskTree_->topLevelItem(i)->text(0) == event.taskId) { row = taskTree_->topLevelItem(i); break; }
    }
    if (!row) {
        if (taskTree_->topLevelItemCount() >= 500) delete taskTree_->takeTopLevelItem(0);
        row = new QTreeWidgetItem(taskTree_, {event.taskId});
    }
    row->setText(1, taskStatus(event.status));
    row->setText(2, event.message);
    row->setText(3, event.timestamp.toLocalTime().toString("HH:mm:ss"));
    monitorStatus_->setText(event.taskId + " · " + taskStatus(event.status));
    if (event.progress.totalSteps > 0) {
        progress_->setValue(qBound(0, static_cast<int>(100.0 * event.progress.currentStep / event.progress.totalSteps), 100));
        monitorDetails_->setText(uiText(u"迭代：%1 / %2    残差：%3").arg(event.progress.currentStep).arg(event.progress.totalSteps).arg(event.progress.residual));
    } else {
        progress_->setValue(0);
        monitorDetails_->setText(uiText(u"暂无迭代 / 残差数据；Mock 消息不代表真实计算"));
    }
    appendMessage(event.taskId + " · " + event.message);
    if (event.type == TaskEventType::Failed || event.type == TaskEventType::Warning)
        errors_->appendPlainText(event.errorCode + " " + event.message);
}

void MainWindow::appendMessage(const QString &message)
{
    log_->appendPlainText(QDateTime::currentDateTime().toString("[HH:mm:ss] ") + message);
}

void MainWindow::showOutput(int index)
{
    outputDock_->show();
    outputDock_->raise();
    outputTabs_->setCurrentIndex(index);
}

} // namespace aerosim
