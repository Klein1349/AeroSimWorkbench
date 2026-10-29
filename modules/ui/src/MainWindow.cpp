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
#include <cstddef>

namespace aerosim {
namespace {
// Match the postprocess UI: explicit UTF-16 literals avoid source/execution
// code-page dependence in the Qt 5.12 / MSVC 2017 toolchain.
template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

QString taskStatus(TaskStatus status)
{
    switch (status) {
    case TaskStatus::Preparing: return uiText(u"\u51c6\u5907\u4e2d");
    case TaskStatus::Running: return uiText(u"\u8fd0\u884c\u4e2d");
    case TaskStatus::Paused: return uiText(u"\u5df2\u6682\u505c");
    case TaskStatus::Finished: return uiText(u"\u5df2\u5b8c\u6210");
    case TaskStatus::Failed: return uiText(u"\u5931\u8d25");
    case TaskStatus::Cancelled: return uiText(u"\u5df2\u53d6\u6d88");
    default: return uiText(u"\u672a\u77e5");
    }
}
QString stepStatus(StepState state)
{
    switch (state) {
    case StepState::Pending: return uiText(u"\u5f85\u914d\u7f6e");
    case StepState::NeedsValidation: return uiText(u"\u5f85\u6821\u9a8c");
    case StepState::Complete: return uiText(u"\u5df2\u5b8c\u6210");
    case StepState::Blocked: return uiText(u"\u88ab\u963b\u585e");
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
    setObjectName(QStringLiteral("AeroSimMainWindow"));
    setWindowTitle(uiText(u"AeroSimWorkbench \u00b7 \u4eff\u771f\u5de5\u4f5c\u53f0\uff08Mock \u670d\u52a1\uff09"));
    resize(1440, 960);
    setMinimumSize(1000, 720);
    setDockNestingEnabled(true);
    createWorkspace();
    createRibbon();
    status_ = new QLabel(uiText(u"\u5c31\u7eea  |  \u5de5\u7a0b\uff1a\u672a\u6253\u5f00  |  \u7f51\u683c\uff1a\u672a\u8f7d\u5165  |  \u6c42\u89e3\u5668\uff1a\u672a\u63a5\u5165  |  MPI\uff1a\u2014"), this);
    statusBar()->addWidget(status_, 1);
    statusBar()->addPermanentWidget(new QLabel(uiText(u"UI \u6846\u67b6 \u00b7 Mock \u670d\u52a1"), this));
    QTimer::singleShot(0, this, [this] {
        setStyleSheet(styleSheet() + QStringLiteral(
            "QDockWidget::title { background: #e9eef5; padding: 7px; color: #25364c; }"
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
    appendMessage(uiText(u"\u4e3b\u754c\u9762\u5df2\u5c31\u7eea\u3002\u5de5\u7a0b\u6811\u4e0e\u53c2\u6570\u4e3a\u754c\u9762\u793a\u4f8b\uff1b\u5c1a\u672a\u63a5\u5165\u5de5\u7a0b\u3001\u914d\u7f6e\u3001VTK \u548c\u771f\u5b9e\u6c42\u89e3\u670d\u52a1\u3002"));
}

void MainWindow::createRibbon()
{
    struct Group { QString title; QString items; };
    const Group groups[] = {
        {uiText(u"\u5de5\u7a0b"), uiText(u"\u65b0\u5efa\u5de5\u7a0b|\u6253\u5f00\u5de5\u7a0b|\u4fdd\u5b58\u5de5\u7a0b|\u5bfc\u5165\u914d\u7f6e|\u5bfc\u51fa\u914d\u7f6e")},
        {uiText(u"\u524d\u5904\u7406"), uiText(u"\u5bfc\u5165\u7f51\u683c|\u7f51\u683c\u53d8\u6362|\u5927\u6c14\u53c2\u6570|\u8fb9\u754c\u6761\u4ef6|\u521d\u59cb\u5316")},
        {uiText(u"\u72b6\u6001\u7ba1\u7406"), uiText(u"\u65b0\u5efa\u72b6\u6001|\u72b6\u6001\u6a21\u677f|\u591a\u72b6\u6001\u914d\u7f6e")},
        {uiText(u"\u6c42\u89e3"), uiText(u"\u6c42\u89e3\u5668\u8bbe\u7f6e|\u5e76\u884c\u8bbe\u7f6e|\u591a\u72b6\u6001\u8ba1\u7b97 / \u4f20\u70ed|\u8026\u5408\u63a7\u5236|\u542f\u52a8|\u6682\u505c|\u7eed\u7b97|\u505c\u6b62")},
        {uiText(u"\u76d1\u63a7"), uiText(u"\u8ba1\u7b97\u72b6\u6001|\u65e5\u5fd7|\u6536\u655b\u66f2\u7ebf|\u4efb\u52a1\u7ba1\u7406")},
        {uiText(u"\u540e\u5904\u7406"), uiText(u"\u540e\u5904\u7406\u5de5\u4f5c\u53f0|\u4e91\u56fe|\u5207\u9762|\u7b49\u503c\u9762|\u6d41\u7ebf|\u66f2\u7ebf|\u52a8\u753b")},
        {uiText(u"\u6570\u636e\u5904\u7406"), uiText(u"\u591a\u72b6\u6001\u6570\u636e\u5904\u7406")},
        {uiText(u"\u5de5\u5177"), uiText(u"\u6062\u590d\u9ed8\u8ba4\u5e03\u5c40|\u5de5\u7a0b\u5bfc\u822a|\u5c5e\u6027\u9762\u677f|\u8f93\u51fa\u9762\u677f")},
        {uiText(u"\u5e2e\u52a9"), uiText(u"\u4f7f\u7528\u8bf4\u660e|\u5173\u4e8e")}
    };
    for (const auto &group : groups) {
        const QString title = group.title;
        auto *category = ribbonBar()->addCategoryPage(title);
        auto *panel = category->addPanel(title + uiText(u"\u64cd\u4f5c"));
        for (const QString &name : group.items.split('|')) {
            QStyle::StandardPixmap icon = QStyle::SP_FileDialogDetailedView;
            if (name.contains(uiText(u"\u65b0\u5efa"))) icon = QStyle::SP_FileIcon;
            else if (name.contains(uiText(u"\u6253\u5f00")) || name.contains(uiText(u"\u5bfc\u5165"))) icon = QStyle::SP_DialogOpenButton;
            else if (name.contains(uiText(u"\u4fdd\u5b58")) || name.contains(uiText(u"\u5bfc\u51fa"))) icon = QStyle::SP_DialogSaveButton;
            else if (name == uiText(u"\u542f\u52a8") || name == uiText(u"\u7eed\u7b97")) icon = QStyle::SP_MediaPlay;
            else if (name == uiText(u"\u6682\u505c")) icon = QStyle::SP_MediaPause;
            else if (name == uiText(u"\u505c\u6b62")) icon = QStyle::SP_MediaStop;
            auto *action = new QAction(style()->standardIcon(icon), name, this);
            panel->addLargeAction(action);
            action->setEnabled(false);
            action->setToolTip(name + uiText(u"\uff1a\u7b49\u5f85\u5bf9\u5e94\u4e1a\u52a1\u6a21\u5757\u63a5\u5165"));
            if (name == uiText(u"\u540e\u5904\u7406\u5de5\u4f5c\u53f0")) {
                action->setObjectName(QStringLiteral("openPostProcessAction"));
                action->setEnabled(true);
                action->setToolTip(uiText(u"\u6253\u5f00\u6570\u636e\u3001\u573a\u666f\u3001\u65f6\u95f4\u8f74\u4e0e\u53ef\u9009 VTK \u89c6\u53e3"));
                connect(action, &QAction::triggered, this, &MainWindow::openPostProcessRequested);
            } else if (name == uiText(u"\u542f\u52a8")) {
                runAction_ = action;
                action->setObjectName(QStringLiteral("runAction"));
                connect(action, &QAction::triggered, this, &MainWindow::submitRun);
            } else if (title == uiText(u"\u76d1\u63a7")) {
                const int index = name == uiText(u"\u65e5\u5fd7") ? 0 : name == uiText(u"\u8ba1\u7b97\u72b6\u6001") ? 1 : name == uiText(u"\u6536\u655b\u66f2\u7ebf") ? 2 : 4;
                action->setEnabled(true);
                action->setToolTip(uiText(u"\u6253\u5f00") + name);
                connect(action, &QAction::triggered, this, [this, index] { showOutput(index); });
            } else if (title == uiText(u"\u5de5\u5177")) {
                action->setEnabled(true);
                action->setToolTip(name);
                connect(action, &QAction::triggered, this, [this, name] {
                    if (name == uiText(u"\u6062\u590d\u9ed8\u8ba4\u5e03\u5c40")) restoreLayout();
                    else if (name == uiText(u"\u5de5\u7a0b\u5bfc\u822a")) projectDock_->show();
                    else if (name == uiText(u"\u5c5e\u6027\u9762\u677f")) propertyDock_->show();
                    else outputDock_->show();
                });
            } else if (title == uiText(u"\u5e2e\u52a9")) {
                action->setEnabled(true);
                action->setToolTip(name);
                connect(action, &QAction::triggered, this, [this] {
                    appendMessage(uiText(u"\u4f7f\u7528\u8bf4\u660e\uff1a\u5de6\u4fa7\u9009\u62e9\u793a\u4f8b\u5bf9\u8c61\uff0c\u53f3\u4fa7\u7f16\u8f91\u672c\u5730\u8349\u7a3f\uff1b\u62d6\u52a8\u9762\u677f\u53ef\u8c03\u6574\u5e03\u5c40\u3002\u5de5\u4f5c\u6d41\u72b6\u6001\u7531\u670d\u52a1\u63d0\u4f9b\u3002\u7070\u8272\u529f\u80fd\u7b49\u5f85\u4e1a\u52a1\u6a21\u5757\u63a5\u5165\uff0cMock \u4efb\u52a1\u4e0d\u6267\u884c\u771f\u5b9e\u6c42\u89e3\u3002"));
                    showOutput(0);
                });
            } else {
                const QHash<QString, QString> pages = {
                    {uiText(u"\u8fb9\u754c\u6761\u4ef6"), "boundary"}, {uiText(u"\u5927\u6c14\u53c2\u6570"), "physics"},
                    {uiText(u"\u6c42\u89e3\u5668\u8bbe\u7f6e"), "solver"}, {uiText(u"\u5e76\u884c\u8bbe\u7f6e"), "parallel"},
                    {uiText(u"\u591a\u72b6\u6001\u914d\u7f6e"), "states"}, {uiText(u"\u7f51\u683c\u53d8\u6362"), "mesh"}};
                if (pages.contains(name)) {
                    action->setEnabled(true);
                    action->setToolTip(uiText(u"\u6253\u5f00\u53c2\u6570\u8349\u7a3f\uff08\u5c1a\u672a\u6301\u4e45\u5316\uff09"));
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
    auto *viewToolbar = new QToolBar(uiText(u"\u89c6\u56fe\u63a7\u5236"), central);
    for (const QString &name : uiText(u"\u9002\u5e94|\u653e\u5927|\u7f29\u5c0f|\u65cb\u8f6c|\u6b63\u89c6 X|\u4fa7\u89c6 Y|\u9876\u89c6 Z|\u8868\u9762|\u7ebf\u6846").split('|')) {
        auto *action = viewToolbar->addAction(name);
        action->setEnabled(false);
        action->setToolTip(uiText(u"\u7b49\u5f85 VTK \u89c6\u53e3\u53ca\u89c6\u56fe\u63a7\u5236\u63a5\u53e3\u63a5\u5165"));
    }
    centerLayout->addWidget(viewToolbar);
    auto *viewport = createSlot(WidgetSlot::Viewport, uiText(u"\u4e09\u7ef4\u5de5\u4f5c\u533a\n\nVTK 8.2 \u89c6\u53e3\u63a5\u5165\u4f4d\u7f6e\n\u5bfc\u5165\u4e0e\u6e32\u67d3\u6a21\u5757\u5c1a\u672a\u63a5\u5165"));
    viewport->setObjectName(QStringLiteral("viewportSlot"));
    centerLayout->addWidget(viewport, 1);
    setCentralWidget(central);

    projectDock_ = new QDockWidget(uiText(u"\u5de5\u7a0b\u4e0e\u5de5\u4f5c\u6d41"), this);
    projectDock_->setObjectName(QStringLiteral("projectDock"));
    navigationTabs_ = new QTabWidget(projectDock_);
    projectTree_ = new QTreeWidget(navigationTabs_);
    projectTree_->setObjectName(QStringLiteral("projectTree"));
    projectTree_->setHeaderHidden(true);
    auto *root = new QTreeWidgetItem(projectTree_, {uiText(u"\u754c\u9762\u793a\u4f8b\uff08\u672a\u6253\u5f00\u5de5\u7a0b\uff09")});
    const QList<QStringList> nodes = {
        {"mesh", uiText(u"\u7f51\u683c Mesh"), "inlet|outlet|wall"},
        {"physics", uiText(u"\u7269\u7406\u6a21\u578b Physics"), uiText(u"\u6c14\u4f53\u6a21\u578b|\u6d41\u52a8\u6a21\u578b|\u6e4d\u6d41\u6a21\u578b")},
        {"motion", uiText(u"\u8fd0\u52a8 Motion"), uiText(u"\u52a8\u7f51\u683c|\u5f3a\u8feb\u8fd0\u52a8|\u516d\u81ea\u7531\u5ea6")},
        {"boundary", uiText(u"\u8fb9\u754c\u6761\u4ef6"), "inlet|outlet|wall"},
        {"solver", uiText(u"\u6c42\u89e3\u5668 Solver"), uiText(u"\u7a7a\u95f4\u79bb\u6563|\u65f6\u95f4\u79bb\u6563|\u8fed\u4ee3\u63a7\u5236")},
        {"parallel", uiText(u"\u5e76\u884c\u8ba1\u7b97"), ""}, {"states", uiText(u"\u72b6\u6001 States"), uiText(u"\u72b6\u6001\u914d\u7f6e\u793a\u4f8b")},
        {"results", uiText(u"\u7ed3\u679c Results"), uiText(u"\u538b\u529b|\u6e29\u5ea6|\u901f\u5ea6|\u5207\u9762|\u6d41\u7ebf")}
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
    navigationTabs_->addTab(projectTree_, uiText(u"\u5de5\u7a0b\u6811"));
    auto *workflowPage = new QWidget(navigationTabs_);
    auto *workflowLayout = new QVBoxLayout(workflowPage);
    suggestion_ = new QLabel(uiText(u"\u7b49\u5f85\u5de5\u4f5c\u6d41\u670d\u52a1"), workflowPage);
    suggestion_->setWordWrap(true);
    workflowLayout->addWidget(suggestion_);
    workflowTree_ = new QTreeWidget(workflowPage);
    workflowTree_->setObjectName(QStringLiteral("workflowTree"));
    workflowTree_->setHeaderLabels({uiText(u"\u6b65\u9aa4 / \u64cd\u4f5c"), uiText(u"\u72b6\u6001")});
    workflowTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    workflowLayout->addWidget(workflowTree_);
    connect(workflowTree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString page = item->data(0, Qt::UserRole).toString();
        if (!page.isEmpty()) selectPage(page);
        emit navigationRequested(page, item->data(0, Qt::UserRole + 1).toString(), item->data(0, Qt::UserRole + 2).toString());
    });
    navigationTabs_->addTab(workflowPage, uiText(u"\u5de5\u4f5c\u6d41"));
    projectDock_->setWidget(navigationTabs_);

    propertyDock_ = new QDockWidget(uiText(u"\u5c5e\u6027\u7f16\u8f91\u5668"), this);
    propertyDock_->setObjectName(QStringLiteral("propertyDock"));
    auto *properties = new QWidget(propertyDock_);
    auto *propertyLayout = new QVBoxLayout(properties);
    propertyTitle_ = new QLabel(properties);
    propertyTitle_->setObjectName(QStringLiteral("propertyTitle"));
    propertyLayout->addWidget(propertyTitle_);
    auto *notice = new QLabel(uiText(u"\u53c2\u6570\u8349\u7a3f\u793a\u4f8b \u00b7 \u4ec5\u4fdd\u5b58\u5728\u5f53\u524d\u7a97\u53e3\n\u6b63\u5f0f\u53c2\u6570\u6821\u9a8c\u4e0e\u5de5\u7a0b\u4fdd\u5b58\u5f85\u63a5\u5165"), properties);
    notice->setWordWrap(true);
    propertyLayout->addWidget(notice);
    propertyForm_ = new QFormLayout;
    propertyForm_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    propertyLayout->addLayout(propertyForm_);
    propertyLayout->addStretch();
    auto *reset = new QPushButton(uiText(u"\u6e05\u9664\u672c\u9875\u8349\u7a3f"), properties);
    connect(reset, &QPushButton::clicked, this, [this] { drafts_.remove(currentPage_); selectPage(currentPage_); });
    propertyLayout->addWidget(reset);
    propertyDock_->setWidget(properties);
    selectPage(QStringLiteral("mesh"));

    outputDock_ = new QDockWidget(uiText(u"\u8f93\u51fa\u4e0e\u8ba1\u7b97\u76d1\u63a7"), this);
    outputDock_->setObjectName(QStringLiteral("outputDock"));
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
    outputTabs_->addTab(logSlot, uiText(u"\u65e5\u5fd7"));
    auto *monitorSlot = createSlot(WidgetSlot::Monitor, {});
    auto *monitorLayout = slotLayouts_.value(static_cast<int>(WidgetSlot::Monitor));
    auto *oldMonitorItem = monitorLayout->takeAt(0);
    delete oldMonitorItem->widget();
    delete oldMonitorItem;
    auto *monitor = new QWidget(monitorSlot);
    auto *monitorForm = new QFormLayout(monitor);
    monitorStatus_ = new QLabel(uiText(u"\u5c1a\u65e0\u4efb\u52a1\u4e8b\u4ef6"), monitor);
    monitorDetails_ = new QLabel(uiText(u"\u8fed\u4ee3\uff1a\u2014    \u6b8b\u5dee\uff1a\u2014    CPU\uff1a\u2014    \u5185\u5b58\uff1a\u2014    MPI\uff1a\u2014"), monitor);
    progress_ = new QProgressBar(monitor);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    monitorForm->addRow(uiText(u"\u4efb\u52a1\u72b6\u6001"), monitorStatus_);
    monitorForm->addRow(uiText(u"\u8ba1\u7b97\u6307\u6807"), monitorDetails_);
    monitorForm->addRow(uiText(u"\u8fdb\u5ea6"), progress_);
    monitorLayout->addWidget(monitor);
    outputTabs_->addTab(monitorSlot, uiText(u"\u8ba1\u7b97\u76d1\u63a7"));
    outputTabs_->addTab(emptyState(uiText(u"\u6536\u655b\u66f2\u7ebf\n\u7b49\u5f85\u6b8b\u5dee\u91c7\u6837\u6570\u636e\u4e0e\u66f2\u7ebf\u7ec4\u4ef6\u63a5\u5165"), outputTabs_), uiText(u"\u6536\u655b\u66f2\u7ebf"));
    auto *errorSlot = createSlot(WidgetSlot::Error, {});
    auto *errorLayout = slotLayouts_.value(static_cast<int>(WidgetSlot::Error));
    auto *oldErrorItem = errorLayout->takeAt(0);
    delete oldErrorItem->widget();
    delete oldErrorItem;
    errors_ = new QPlainTextEdit(this);
    errors_->setReadOnly(true);
    errors_->setMaximumBlockCount(1000);
    errorLayout->addWidget(errors_);
    outputTabs_->addTab(errorSlot, uiText(u"\u9519\u8bef\u4fe1\u606f"));
    taskTree_ = new QTreeWidget(outputTabs_);
    taskTree_->setObjectName(QStringLiteral("taskTree"));
    taskTree_->setHeaderLabels({uiText(u"\u4efb\u52a1 ID"), uiText(u"\u72b6\u6001"), uiText(u"\u6d88\u606f"), uiText(u"\u65f6\u95f4")});
    taskTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    taskTree_->setAlternatingRowColors(true);
    outputTabs_->addTab(taskTree_, uiText(u"\u4efb\u52a1\u5217\u8868"));
    outputDock_->setWidget(outputTabs_);
}

void MainWindow::selectPage(const QString &pageId)
{
    currentPage_ = pageId;
    while (propertyForm_->rowCount()) propertyForm_->removeRow(0);
    const QHash<QString, QString> titles = {{"mesh", uiText(u"\u7f51\u683c\u4e0e\u53d8\u6362")}, {"physics", uiText(u"\u7269\u7406\u6a21\u578b / \u5927\u6c14\u53c2\u6570")},
        {"motion", uiText(u"\u8fd0\u52a8\u53c2\u6570")}, {"boundary", uiText(u"\u8fb9\u754c\u6761\u4ef6")}, {"solver", uiText(u"\u6c42\u89e3\u5668\u8bbe\u7f6e")},
        {"parallel", uiText(u"\u5e76\u884c\u8bbe\u7f6e")}, {"states", uiText(u"\u5de5\u51b5\u72b6\u6001")}, {"results", uiText(u"\u7ed3\u679c\u4e0e\u540e\u5904\u7406")}};
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
        type->addItems({uiText(u"\u538b\u529b\u5165\u53e3"), uiText(u"\u901f\u5ea6\u5165\u53e3"), uiText(u"\u538b\u529b\u51fa\u53e3"), uiText(u"\u58c1\u9762")});
        type->setCurrentIndex(drafts_.value(pageId).value("type", 0).toInt());
        propertyForm_->addRow(uiText(u"\u8fb9\u754c\u7c7b\u578b\uff08\u793a\u4f8b\uff09"), type);
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, pageId](int value) {
            drafts_[pageId].insert("type", value);
            emit parameterDraftChanged(pageId, drafts_.value(pageId));
        });
        addNumber("pressure", uiText(u"\u538b\u529b"), 101325, " Pa");
        addNumber("temperature", uiText(u"\u6e29\u5ea6"), 300, " K");
    } else if (pageId == "motion" || pageId == "mesh") {
        if (pageId == "motion") addNumber("mass", uiText(u"\u8d28\u91cf"), 15.2, " kg");
        for (const QString &axis : {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")})
            addNumber(axis, uiText(u"\u4f4d\u7f6e ") + axis, 0, " m");
    } else if (pageId == "solver") {
        addNumber("iterations", uiText(u"\u6700\u5927\u8fed\u4ee3\u6b65"), 10000, {});
        addNumber("timeStep", uiText(u"\u65f6\u95f4\u6b65\u957f"), 0.001, " s");
    } else if (pageId == "parallel") {
        addNumber("processes", uiText(u"MPI \u8fdb\u7a0b\u6570"), 32, {});
    } else {
        auto *note = new QLabel(pageId == "results" ? uiText(u"\u5c1a\u65e0\u7ed3\u679c\u6570\u636e\u3002\u7ed3\u679c\u5339\u914d\u6821\u9a8c\u548c\u663e\u793a\u7531\u5de5\u4f5c\u6d41\u53ca\u53ef\u89c6\u5316\u6a21\u5757\u63d0\u4f9b\u3002") : uiText(u"\u9009\u62e9\u914d\u7f6e\u5bf9\u8c61\u540e\u7531\u4e13\u4e1a\u6a21\u5757\u63d0\u4f9b\u53c2\u6570\u3002"), this);
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
            suggestion_->setText(uiText(u"\u5de5\u4f5c\u6d41\u670d\u52a1\u5df2\u65ad\u5f00"));
        }));
    } else {
        workflowTree_->clear();
        suggestion_->setText(uiText(u"\u7b49\u5f85\u5de5\u4f5c\u6d41\u670d\u52a1"));
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
        auto *item = new QTreeWidgetItem(workflowTree_, {action.actionId, action.enabled ? uiText(u"\u53ef\u7528") : uiText(u"\u4e0d\u53ef\u7528")});
        item->setToolTip(0, action.reason);
        item->setData(0, Qt::UserRole, action.targetPageId);
        item->setData(0, Qt::UserRole + 1, action.objectId);
        item->setData(0, Qt::UserRole + 2, action.fieldKey);
        if (action.actionId == "run" && runAction_) {
            runAction_->setEnabled(action.enabled && hasRunContext_ && workflow_ && workflow_->thread() == thread());
            runAction_->setToolTip(action.reason.isEmpty() ? uiText(u"\u5411\u5de5\u4f5c\u6d41\u670d\u52a1\u8bf7\u6c42\u8fd0\u884c") : action.reason);
        }
    }
    for (const auto &issue : view.issues) {
        auto *item = new QTreeWidgetItem(workflowTree_, {issue.message, uiText(u"\u6821\u9a8c\u63d0\u793a")});
        item->setData(0, Qt::UserRole, issue.targetPageId);
        item->setData(0, Qt::UserRole + 1, issue.objectId);
        item->setData(0, Qt::UserRole + 2, issue.fieldKey);
    }
}

void MainWindow::submitRun()
{
    if (!workflow_ || workflow_->thread() != thread() || !hasRunContext_) return;
    const auto result = workflow_->requestRun(runRequest_);
    appendMessage(result.allowed ? uiText(u"\u8fd0\u884c\u8bf7\u6c42\u5df2\u63d0\u4ea4\uff0c\u7b49\u5f85\u4efb\u52a1\u4e8b\u4ef6") : uiText(u"\u5de5\u4f5c\u6d41\u62d2\u7edd\u8fd0\u884c\u8bf7\u6c42"));
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
    monitorStatus_->setText(event.taskId + uiText(u" \u00b7 ") + taskStatus(event.status));
    if (event.progress.totalSteps > 0) {
        progress_->setValue(qBound(0, static_cast<int>(100.0 * event.progress.currentStep / event.progress.totalSteps), 100));
        monitorDetails_->setText(uiText(u"\u8fed\u4ee3\uff1a%1 / %2    \u6b8b\u5dee\uff1a%3").arg(event.progress.currentStep).arg(event.progress.totalSteps).arg(event.progress.residual));
    } else {
        progress_->setValue(0);
        monitorDetails_->setText(uiText(u"\u6682\u65e0\u8fed\u4ee3 / \u6b8b\u5dee\u6570\u636e\uff1bMock \u6d88\u606f\u4e0d\u4ee3\u8868\u771f\u5b9e\u8ba1\u7b97"));
    }
    appendMessage(event.taskId + uiText(u" \u00b7 ") + event.message);
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
