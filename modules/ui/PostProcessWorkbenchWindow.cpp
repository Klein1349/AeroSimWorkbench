#include "aerosim/ui/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace aerosim {
namespace {

QToolButton *addToolButton(QToolBar *toolbar, const QString &text, const QString &toolTip)
{
    auto *button = new QToolButton(toolbar);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->addWidget(button);
    return button;
}

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

} // namespace

PostProcessWorkbenchWindow::PostProcessWorkbenchWindow(QWidget *parent) : QMainWindow(parent)
{
    setObjectName(QStringLiteral("postProcessWorkbench"));
    setWindowTitle(QStringLiteral("AeroSimWorkbench - 后处理工作台"));
    resize(1440, 900);
    setMinimumSize(1120, 680);
    createActions();

    auto *workbench = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(workbench);
    rootLayout->setContentsMargins(6, 6, 6, 0);
    rootLayout->setSpacing(6);

    auto *content = new QSplitter(Qt::Horizontal, workbench);
    content->setObjectName(QStringLiteral("postProcessContent"));
    auto *leftTabs = new QTabWidget(content);
    leftTabs->setObjectName(QStringLiteral("postProcessNavigator"));
    leftTabs->addTab(createDataPanel(), QStringLiteral("数据"));
    leftTabs->addTab(createScenePanel(), QStringLiteral("场景"));
    content->addWidget(leftTabs);
    content->addWidget(createViewportPanel());
    content->addWidget(createPropertiesPanel());
    content->setStretchFactor(0, 0);
    content->setStretchFactor(1, 1);
    content->setStretchFactor(2, 0);
    content->setSizes({260, 850, 300});
    rootLayout->addWidget(content, 1);
    rootLayout->addWidget(createTimeline());
    setCentralWidget(workbench);

    auto *messageDock = new QDockWidget(QStringLiteral("后处理消息"), this);
    messageDock->setObjectName(QStringLiteral("postProcessMessageDock"));
    events_ = new QTextEdit(messageDock);
    events_->setObjectName(QStringLiteral("postProcessEvents"));
    events_->setReadOnly(true);
    events_->setMinimumHeight(100);
    messageDock->setWidget(events_);
    addDockWidget(Qt::BottomDockWidgetArea, messageDock);
    messageDock->hide();

    statusBar()->showMessage(QStringLiteral("后处理工作台已就绪：等待结果数据与 VTK 场景服务接入"));
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #edf1f5; }"
        "QToolBar { background: #ffffff; border-bottom: 1px solid #cbd4df; spacing: 8px; padding: 4px; }"
        "QToolButton { min-width: 52px; min-height: 40px; color: #233246; }"
        "QToolButton:hover { background: #e4eef9; border-radius: 4px; }"
        "QTabWidget::pane, QFrame#viewportFrame, QGroupBox { background: #ffffff; border: 1px solid #cbd4df; }"
        "QTabBar::tab { padding: 8px 18px; background: #e6ebf1; }"
        "QTabBar::tab:selected { background: #ffffff; color: #0b5ca8; }"
        "QListWidget { border: 0; background: #ffffff; outline: 0; }"
        "QListWidget::item { min-height: 34px; padding: 5px 8px; border-bottom: 1px solid #edf1f5; }"
        "QListWidget::item:selected { background: #e4eef9; color: #0b5ca8; }"
        "QFrame#dataCard { background: #f5f8fc; border: 1px solid #d9e3ee; border-radius: 5px; }"
        "QPushButton#primaryAction { background: #0b69b7; color: white; border: 0; border-radius: 4px; padding: 5px 10px; }"
        "QLabel#mutedText { color: #6e7f91; font-size: 12px; }"
        "QLabel#sectionTitle { color: #31445d; font-weight: bold; padding: 5px 2px; }"
        "QGroupBox { margin-top: 12px; padding: 8px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QTextEdit { background: #182432; color: #cfe2f5; border: 0; }"));
}

void PostProcessWorkbenchWindow::createActions()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
    auto *importAction = fileMenu->addAction(QStringLiteral("导入结果…"));
    auto *exportAction = fileMenu->addAction(QStringLiteral("导出当前对象…"));
    auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    auto *fitAction = viewMenu->addAction(QStringLiteral("适配视图"));
    auto *featureMenu = menuBar()->addMenu(QStringLiteral("特征"));
    auto *sliceAction = featureMenu->addAction(QStringLiteral("创建切面"));
    auto *isoAction = featureMenu->addAction(QStringLiteral("创建等值面"));
    auto *streamAction = featureMenu->addAction(QStringLiteral("创建流线"));
    auto *analysisMenu = menuBar()->addMenu(QStringLiteral("分析"));
    auto *curveAction = analysisMenu->addAction(QStringLiteral("打开曲线分析"));

    auto *toolbar = addToolBar(QStringLiteral("后处理"));
    toolbar->setObjectName(QStringLiteral("postProcessToolbar"));
    auto *importButton = addToolButton(toolbar, QStringLiteral("导入结果"), QStringLiteral("导入一个或多个结果文件"));
    auto *meshButton = addToolButton(toolbar, QStringLiteral("网格"), QStringLiteral("切换网格和边线显示"));
    auto *contourButton = addToolButton(toolbar, QStringLiteral("云图"), QStringLiteral("选择场变量并显示云图"));
    auto *sliceButton = addToolButton(toolbar, QStringLiteral("切面"), QStringLiteral("创建独立的切面对象"));
    auto *isoButton = addToolButton(toolbar, QStringLiteral("等值面"), QStringLiteral("创建三维等值面"));
    auto *streamButton = addToolButton(toolbar, QStringLiteral("流线"), QStringLiteral("设置种子并创建流线"));
    auto *fitButton = addToolButton(toolbar, QStringLiteral("适配视图"), QStringLiteral("使场景适配当前视口"));
    auto *exportButton = addToolButton(toolbar, QStringLiteral("导出"), QStringLiteral("导出特征、图像或动画"));

    auto command = [this](const QString &name, const QString &message) {
        emit postProcessCommandRequested(name);
        appendMessage(message);
    };
    connect(importAction, &QAction::triggered, this, [command] { command(QStringLiteral("importResults"), QStringLiteral("导入结果请求已提交；读取服务接入后将在工作线程加载。")); });
    connect(exportAction, &QAction::triggered, this, [command] { command(QStringLiteral("exportObject"), QStringLiteral("导出请求已提交；等待选中的场景对象。")); });
    connect(fitAction, &QAction::triggered, this, [this] { emit viewRequested(QStringLiteral("reset")); appendMessage(QStringLiteral("视图已重置。")); });
    connect(sliceAction, &QAction::triggered, this, [command] { command(QStringLiteral("createSlice"), QStringLiteral("创建切面请求已提交。")); });
    connect(isoAction, &QAction::triggered, this, [command] { command(QStringLiteral("createIsoSurface"), QStringLiteral("创建等值面请求已提交。")); });
    connect(streamAction, &QAction::triggered, this, [command] { command(QStringLiteral("createStreamline"), QStringLiteral("创建流线请求已提交。")); });
    connect(curveAction, &QAction::triggered, this, [command] { command(QStringLiteral("openCurveAnalysis"), QStringLiteral("曲线分析工作区请求已提交。")); });
    connect(importButton, &QToolButton::clicked, importAction, &QAction::trigger);
    connect(meshButton, &QToolButton::clicked, this, [command] { command(QStringLiteral("toggleMesh"), QStringLiteral("网格显示层切换请求已提交。")); });
    connect(contourButton, &QToolButton::clicked, this, [command] { command(QStringLiteral("toggleContour"), QStringLiteral("云图显示层切换请求已提交。")); });
    connect(sliceButton, &QToolButton::clicked, sliceAction, &QAction::trigger);
    connect(isoButton, &QToolButton::clicked, isoAction, &QAction::trigger);
    connect(streamButton, &QToolButton::clicked, streamAction, &QAction::trigger);
    connect(fitButton, &QToolButton::clicked, fitAction, &QAction::trigger);
    connect(exportButton, &QToolButton::clicked, exportAction, &QAction::trigger);
}

QWidget *PostProcessWorkbenchWindow::createDataPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(7, 7, 7, 7);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(sectionLabel(QStringLiteral("结果数据"), panel));
    titleRow->addStretch();
    auto *addResult = new QPushButton(QStringLiteral("＋ 导入"), panel);
    addResult->setObjectName(QStringLiteral("primaryAction"));
    titleRow->addWidget(addResult);
    layout->addLayout(titleRow);
    auto *datasetCard = new QFrame(panel);
    datasetCard->setObjectName(QStringLiteral("dataCard"));
    auto *cardLayout = new QVBoxLayout(datasetCard);
    auto *datasetName = new QLabel(QStringLiteral("尚未加载结果"), datasetCard);
    datasetName->setStyleSheet(QStringLiteral("font-weight: 600; color: #25364c;"));
    auto *datasetStatus = new QLabel(QStringLiteral("导入 CGNS、VTK 或 Tecplot 数据文件"), datasetCard);
    datasetStatus->setObjectName(QStringLiteral("mutedText"));
    cardLayout->addWidget(datasetName);
    cardLayout->addWidget(datasetStatus);
    layout->addWidget(datasetCard);
    auto *categories = new QListWidget(panel);
    categories->setObjectName(QStringLiteral("dataCategories"));
    categories->addItems({QStringLiteral("▦  网格与 Zone"), QStringLiteral("◉  物理场"), QStringLiteral("◷  时间序列")});
    categories->setCurrentRow(0);
    layout->addWidget(categories);
    auto *hint = new QLabel(QStringLiteral("数据按类型浏览，不使用层级树。读取状态由数据服务异步更新。"), panel);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #66788a; padding: 4px;"));
    layout->addWidget(hint);
    connect(addResult, &QPushButton::clicked, this, [this] {
        emit postProcessCommandRequested(QStringLiteral("importResults"));
        appendMessage(QStringLiteral("导入结果请求已提交；读取服务接入后将在工作线程加载。"));
    });
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createScenePanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(7, 7, 7, 7);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(sectionLabel(QStringLiteral("显示图层"), panel));
    titleRow->addStretch();
    auto *newLayer = new QPushButton(QStringLiteral("＋ 新建"), panel);
    titleRow->addWidget(newLayer);
    layout->addLayout(titleRow);
    auto *subtitle = new QLabel(QStringLiteral("图层是平铺对象，可单独显隐和编辑。"), panel);
    subtitle->setObjectName(QStringLiteral("mutedText"));
    layout->addWidget(subtitle);
    sceneList_ = new QListWidget(panel);
    sceneList_->setObjectName(QStringLiteral("sceneLayerList"));
    sceneList_->setSelectionMode(QAbstractItemView::SingleSelection);
    const QStringList layers = {QStringLiteral("◉  原始网格     可见"), QStringLiteral("◌  云图         未创建"),
                                QStringLiteral("╱  切面         未创建"), QStringLiteral("◇  等值面       未创建"),
                                QStringLiteral("≈  流线         未创建")};
    sceneList_->addItems(layers);
    sceneList_->setCurrentRow(0);
    layout->addWidget(sceneList_, 1);
    auto *actions = new QHBoxLayout;
    auto *showButton = new QPushButton(QStringLiteral("显隐"), panel);
    auto *removeButton = new QPushButton(QStringLiteral("删除"), panel);
    actions->addWidget(showButton);
    actions->addWidget(removeButton);
    layout->addLayout(actions);
    connect(newLayer, &QPushButton::clicked, this, [this] {
        emit postProcessCommandRequested(QStringLiteral("newLayer"));
        appendMessage(QStringLiteral("新建显示图层请求已提交。"));
    });
    connect(sceneList_, &QListWidget::itemSelectionChanged, this, &PostProcessWorkbenchWindow::updateSelectionDetails);
    connect(showButton, &QPushButton::clicked, this, [this] { appendMessage(QStringLiteral("场景对象显隐请求已记录。")); });
    connect(removeButton, &QPushButton::clicked, this, [this] { appendMessage(QStringLiteral("场景对象删除请求已记录。")); });
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewportPanel()
{
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("viewportFrame"));
    viewportLayout_ = new QVBoxLayout(frame);
    viewportLayout_->setContentsMargins(0, 0, 0, 0);

    emptyViewport_ = new QWidget(frame);
    emptyViewport_->setObjectName(QStringLiteral("vtkViewportPlaceholder"));
    auto *layout = new QVBoxLayout(emptyViewport_);
    layout->setContentsMargins(20, 20, 20, 20);
    auto *viewTools = new QHBoxLayout;
    auto *viewTitle = new QLabel(QStringLiteral("三维结果视口"), emptyViewport_);
    viewTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #e8f0f8;"));
    viewTools->addWidget(viewTitle);
    viewTools->addStretch();
    for (const QString &text : {QStringLiteral("正视"), QStringLiteral("俯视"), QStringLiteral("侧视"), QStringLiteral("重置")}) {
        auto *button = new QPushButton(text, emptyViewport_);
        button->setObjectName(QStringLiteral("viewportControl"));
        viewTools->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, text] {
            const QString orientation = text == QStringLiteral("正视") ? QStringLiteral("front")
                : text == QStringLiteral("俯视") ? QStringLiteral("top")
                : text == QStringLiteral("侧视") ? QStringLiteral("side") : QStringLiteral("reset");
            emit viewRequested(orientation);
            appendMessage(QStringLiteral("%1视图命令已提交。").arg(text));
        });
    }
    layout->addLayout(viewTools);
    layout->addStretch();
    auto *placeholder = new QLabel(QStringLiteral("VTK 三维视口\n\n结果加载后在此显示网格、云图、切面、等值面和流线"), emptyViewport_);
    placeholder->setObjectName(QStringLiteral("viewportEmptyText"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral("font-size: 18px; color: #c9d6e4;"));
    layout->addWidget(placeholder, 1);
    auto *overlay = new QLabel(QStringLiteral("X  →     Y  ↑     Z  ⊙"), emptyViewport_);
    overlay->setStyleSheet(QStringLiteral("color: #9fb5c9;"));
    layout->addWidget(overlay, 0, Qt::AlignRight);
    emptyViewport_->setStyleSheet(QStringLiteral("background: #182432;"));
    viewportLayout_->addWidget(emptyViewport_);
    return frame;
}

QWidget *PostProcessWorkbenchWindow::createPropertiesPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(7, 7, 7, 7);
    layout->addWidget(sectionLabel(QStringLiteral("属性与显示"), panel));
    propertiesStack_ = new QStackedWidget(panel);
    propertiesStack_->setObjectName(QStringLiteral("postProcessProperties"));

    auto *empty = new QLabel(QStringLiteral("在左侧图层中选择对象\n以编辑其显示与参数。"), propertiesStack_);
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    propertiesStack_->addWidget(empty);

    auto *details = new QWidget(propertiesStack_);
    auto *detailsLayout = new QVBoxLayout(details);
    auto *identity = new QGroupBox(QStringLiteral("对象"), details);
    auto *identityForm = new QFormLayout(identity);
    selectionName_ = new QLineEdit(identity);
    selectionName_->setReadOnly(true);
    identityForm->addRow(QStringLiteral("名称"), selectionName_);
    auto *visible = new QCheckBox(QStringLiteral("在三维视口中显示"), identity);
    visible->setChecked(true);
    identityForm->addRow(visible);
    detailsLayout->addWidget(identity);

    auto *display = new QGroupBox(QStringLiteral("显示"), details);
    auto *displayForm = new QFormLayout(display);
    auto *field = new QComboBox(display);
    field->addItems({QStringLiteral("未选择字段"), QStringLiteral("压力"), QStringLiteral("速度模"), QStringLiteral("温度")});
    auto *style = new QComboBox(display);
    style->addItems({QStringLiteral("云图"), QStringLiteral("网格线"), QStringLiteral("表面"), QStringLiteral("边线"), QStringLiteral("矢量")});
    auto *opacity = new QSlider(Qt::Horizontal, display);
    opacity->setRange(0, 100);
    opacity->setValue(100);
    displayForm->addRow(QStringLiteral("字段"), field);
    displayForm->addRow(QStringLiteral("显示方式"), style);
    displayForm->addRow(QStringLiteral("不透明度"), opacity);
    detailsLayout->addWidget(display);

    auto *parameters = new QGroupBox(QStringLiteral("特征参数"), details);
    auto *parameterForm = new QFormLayout(parameters);
    auto *value = new QLineEdit(QStringLiteral("待选择对象"), parameters);
    value->setReadOnly(true);
    parameterForm->addRow(QStringLiteral("切面 / 等值值"), value);
    detailsLayout->addWidget(parameters);
    detailsLayout->addStretch();
    propertiesStack_->addWidget(details);
    layout->addWidget(propertiesStack_, 1);
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createTimeline()
{
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("postProcessTimeline"));
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->addWidget(new QLabel(QStringLiteral("时间序列"), frame));
    for (const QString &text : {QStringLiteral("|◀"), QStringLiteral("◀"), QStringLiteral("▶"), QStringLiteral("▶|")}) {
        auto *button = new QPushButton(text, frame);
        button->setFixedWidth(36);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this] { appendMessage(QStringLiteral("时间帧切换请求已记录。")); });
    }
    timeSlider_ = new QSlider(Qt::Horizontal, frame);
    timeSlider_->setObjectName(QStringLiteral("postProcessTimeSlider"));
    timeSlider_->setRange(0, 0);
    timeSlider_->setEnabled(false);
    layout->addWidget(timeSlider_, 1);
    timeLabel_ = new QLabel(QStringLiteral("未加载时序结果"), frame);
    layout->addWidget(timeLabel_);
    return frame;
}

bool PostProcessWorkbenchWindow::insertWidget(WidgetSlot slot, QWidget *widget)
{
    if (slot != WidgetSlot::Viewport || !widget || !viewportLayout_)
        return false;
    if (emptyViewport_) {
        viewportLayout_->removeWidget(emptyViewport_);
        emptyViewport_->deleteLater();
        emptyViewport_ = nullptr;
    }
    if (viewportLayout_->count() != 0)
        return false;
    viewportLayout_->addWidget(widget);
    return true;
}

void PostProcessWorkbenchWindow::bindServices(const ServiceBindings &services)
{
    if (bound_)
        return;
    services_ = services;
    bound_ = true;
    if (services_.tasks) {
        connect(services_.tasks, &ITaskService::taskEventOccurred, this,
                [this](const TaskEvent &event) { appendMessage(event.message); },
                Qt::QueuedConnection);
    }
}

void PostProcessWorkbenchWindow::appendMessage(const QString &message)
{
    events_->append(message.toHtmlEscaped());
    statusBar()->showMessage(message, 5000);
}

void PostProcessWorkbenchWindow::updateSelectionDetails()
{
    const auto selected = sceneList_->selectedItems();
    if (selected.isEmpty()) {
        propertiesStack_->setCurrentIndex(0);
        return;
    }
    selectionName_->setText(selected.first()->text());
    propertiesStack_->setCurrentIndex(1);
}

} // namespace aerosim
