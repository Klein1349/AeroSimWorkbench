#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTextDocument>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QPixmap>
#include <QSlider>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QTimer>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>

#include <functional>

static QWidget *initializePostProcessResources(QWidget *parent)
{
    Q_INIT_RESOURCE(PostProcessIcons);
    Q_INIT_RESOURCE(SARibbonResource);
    return parent;
}

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{ return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1); }

QLabel *sectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent); label->setObjectName(uiText(u"sectionTitle")); return label;
}
// Render the same SVG line artwork at each requested size, including high DPI.
QIcon commandIcon(const QString &kind) {
    static const QMap<QString, QString> aliases = {
        {"import", "import-config"}, {"export", "export-config"},
        {"trace", "animation"}, {"analysis", "curve"}, {"add", "new-state"}};
    const QString key = aliases.value(kind, kind);
    QSvgRenderer renderer(QStringLiteral(":/aerosim/postprocess/") + key + QStringLiteral(".svg"));
    if (!renderer.isValid()) return QIcon();
    QIcon icon;
    for (int size : {16, 20, 24, 32, 40, 48, 64, 96}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

QWidget *scrollPanel(QWidget *contents) {
    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(contents);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

// Native QDockWidget title metrics do not include QSS padding on Qt 5.12.
// A real layout gives the caption and controls their full font-derived height.
void installDockTitle(QDockWidget *dock) {
    auto *bar = new QWidget(dock);
    bar->setObjectName(QStringLiteral("postProcessDockTitle"));
    bar->setStyleSheet(QStringLiteral(
        "QWidget#postProcessDockTitle { background:#e9eef5; }"
        "QLabel { font:9pt 'Microsoft YaHei UI'; color:#25364c; }"
        "QToolButton { border:0; background:transparent; padding:3px; }"
        "QToolButton:hover { background:#dceafd; }"));
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8,3,4,3);
    layout->setSpacing(2);
    auto *label = new QLabel(dock->windowTitle(), bar);
    label->setObjectName(QStringLiteral("postProcessDockCaption"));
    layout->addWidget(label, 1);
    auto *floating = new QToolButton(bar);
    floating->setIcon(commandIcon("dock")); floating->setIconSize(QSize(16,16));
    floating->setToolTip(uiText(u"切换浮动 / 停靠"));
    auto *close = new QToolButton(bar);
    close->setIcon(commandIcon("close")); close->setIconSize(QSize(16,16));
    close->setToolTip(uiText(u"关闭面板"));
    layout->addWidget(floating); layout->addWidget(close);
    QObject::connect(floating, &QToolButton::clicked, dock, [dock] { dock->setFloating(!dock->isFloating()); });
    QObject::connect(close, &QToolButton::clicked, dock, &QWidget::hide);
    dock->setTitleBarWidget(bar);
}

QAction *ribbonAction(QWidget *owner, const QIcon &icon, const QString &text,
                      const QString &tip, const QString &objectName) {
    auto *action = new QAction(icon, text, owner);
    action->setObjectName(objectName); action->setToolTip(tip); action->setProperty("availableToolTip", tip);
    return action;
}

// Shown only until a real VTK widget is inserted. This is a visual preview, not a fake data source.
class ResultPreviewWidget final : public QWidget {
public:
    explicit ResultPreviewWidget(QWidget *parent = nullptr) : QWidget(parent) { setMinimumSize(280, 200); }
protected:
    void paintEvent(QPaintEvent *) override {
        // Keep the preview illustrative, but use the same light workspace
        // surface as the main window's viewport placeholder.
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.fillRect(rect(), QColor("#edf2f8"));
        const QRect area = rect().adjusted(28, 58, -86, -48);
        QLinearGradient gradient(area.topLeft(), area.bottomRight());
        gradient.setColorAt(0, QColor("#174f88")); gradient.setColorAt(.32, QColor("#10a4bd")); gradient.setColorAt(.58, QColor("#79c76f")); gradient.setColorAt(.78, QColor("#f6c84b")); gradient.setColorAt(1, QColor("#d94e4e"));
        QPainterPath body; body.moveTo(area.left() + 12, area.center().y());
        body.cubicTo(area.left() + area.width() * .27, area.top() + 14, area.left() + area.width() * .68, area.top() + 24, area.right() - 12, area.center().y() - 8);
        body.cubicTo(area.left() + area.width() * .68, area.bottom() - 20, area.left() + area.width() * .25, area.bottom() - 24, area.left() + 12, area.center().y());
        p.setBrush(gradient); p.setPen(Qt::NoPen); p.drawPath(body);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor("#c4d2e0"), 1));
        for (int i = 1; i < 9; ++i) { const int x = area.left() + area.width() * i / 9; p.drawLine(x, area.top() + 30, x, area.bottom() - 30); }
        for (int i = 1; i < 6; ++i) { const int y = area.top() + area.height() * i / 6; p.drawLine(area.left() + 24, y, area.right() - 24, y); }
        p.setPen(QPen(QColor("#234d7a"), 2)); p.drawPath(body);
        const QRect legend(rect().right() - 45, 80, 13, qMax(40, height() - 175)); QLinearGradient lg(legend.topLeft(), legend.bottomLeft());
        lg.setColorAt(0, QColor("#d94e4e")); lg.setColorAt(.35, QColor("#f6c84b")); lg.setColorAt(.67, QColor("#10a4bd")); lg.setColorAt(1, QColor("#174f88")); p.fillRect(legend, lg); p.setPen(QColor("#8fa5bb")); p.drawRect(legend);
        p.setPen(QColor("#25364c")); p.drawText(QRect(20, 14, width() - 50, 30), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"三维结果预览"));
        p.setPen(QColor("#52677f")); p.drawText(QRect(20, height() - 34, width() - 40, 20), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"翼身组合 / 压力场 / 时间步 0"));
        p.drawText(QRect(width() - 65, 52, 50, 24), Qt::AlignCenter, uiText(u"压力"));
    }
};
}

PostProcessWorkbenchWindow::PostProcessWorkbenchWindow(QWidget *parent)
    : SARibbonMainWindow(initializePostProcessResources(parent)) {
    setObjectName(uiText(u"postProcessWorkbench")); setWindowTitle(uiText(u"AeroSimWorkbench · 后处理工作台")); resize(1440, 960); setMinimumSize(1120, 680);
    QFont workbenchFont(uiText(u"Microsoft YaHei UI")); workbenchFont.setPointSize(9); setFont(workbenchFont);
    setRibbonTheme(SARibbonTheme::RibbonThemeOffice2013);
    ribbonBar()->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);
    ribbonBar()->applicationButton()->hide();
    ribbonBar()->setWindowTitleBackgroundBrush(QColor("#e5e4e4"));
    ribbonBar()->setWindowTitleTextColor(QColor("#222222"));
    createActions();
    auto *workbench = new QWidget(this); auto *root = new QVBoxLayout(workbench); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    workbench->setObjectName(QStringLiteral("postProcessWorkspace"));
    auto *content = new QSplitter(Qt::Horizontal, workbench); content->setObjectName(uiText(u"postProcessContent"));
    auto *tabs = new QTabWidget(content); tabs->setObjectName(uiText(u"postProcessNavigator")); tabs->setMinimumWidth(250);
    tabs->addTab(scrollPanel(createViewPanel()), uiText(u"视图")); tabs->addTab(createDataPanel(), uiText(u"数据")); tabs->addTab(createObjectPanel(), uiText(u"对象"));
    content->addWidget(tabs); content->addWidget(createViewportPanel()); content->setStretchFactor(1, 1); content->setSizes({270, 1170}); content->setChildrenCollapsible(false);
    root->addWidget(content, 1); root->addWidget(createTimeline()); setCentralWidget(workbench);
    auto *dock = new QDockWidget(uiText(u"任务与消息"), this); taskDock_ = dock; dock->setObjectName(uiText(u"postProcessMessageDock")); events_ = new QTextEdit(dock); events_->setReadOnly(true); events_->document()->setMaximumBlockCount(2000); events_->setPlaceholderText(uiText(u"暂无任务事件。读取、计算和导出服务的进度与错误将在这里显示。")); dock->setWidget(events_); installDockTitle(dock); addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide();
    statusBar()->showMessage(uiText(u"已加载翼身组合结果集：从左侧选择数据，再在 Ribbon 中创建显示或分析对象。"));
    // Scope workspace styling so SARibbon's buttons, tabs and window controls
    // retain their own metrics. Native inputs use the same style as MainWindow.
    workbench->setStyleSheet(QStringLiteral(
        "QWidget { font-family: 'Microsoft YaHei UI'; font-size:9pt; }"
        "QWidget#postProcessWorkspace { background:#e9eef5; color:#25364c; }"
        "QTabWidget::pane { border:0; background:#efefef; }"
        "QTabBar::tab { background:#e8e8e8; color:#222; border:1px solid #bfc4cb; padding:5px 12px; margin-right:1px; }"
        "QTabBar::tab:selected { background:#fff; border-bottom-color:#fff; }"
        "QTabBar::tab:hover:!selected { background:#e0eaf6; }"
        "QLineEdit,QComboBox { background:#fff; color:#222; border:1px solid #aeb5be; padding:4px 6px; }"
        "QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:22px; border:0; }"
        "QComboBox::down-arrow { image:url(:/aerosim/postprocess/dropdown.svg); width:10px; height:6px; }"
        "QLineEdit:focus,QComboBox:focus { border-color:#2563b6; }"
        "QPushButton { background:#fafafa; color:#222; border:1px solid #aeb5be; padding:5px 8px; }"
        "QPushButton:hover { background:#e0eaf6; border-color:#8baed7; }"
        "QPushButton:pressed { background:#dceafd; }"
        "QPushButton:focus { border-color:#2563b6; }"
        "QScrollArea { border:0; background:#efefef; }"
        "QSplitter::handle { background:#e9eef5; }"
        "QFrame#viewportFrame { border:0; background:#edf2f8; }"
        "QFrame#viewportHeader { border:0; background:#e9eef5; }"
        "QLabel#sectionTitle,QLabel#viewportTitle { color:#234d7a; font-weight:600; }"
        "QLabel#mutedText { color:#52677f; }"
        "QFrame#dataCard { border:0; background:transparent; }"
        "QTreeWidget,QListWidget { border:0; background:white; alternate-background-color:#f4f7fb; }"
        "QTreeWidget::item,QListWidget::item { padding:5px 2px; }"
        "QTreeWidget::item:selected,QListWidget::item:selected { background:#dceafd; color:#153b6b; }"
        "QGroupBox { border:0; margin-top:22px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:0; top:3px; color:#234d7a; }"

        "QFrame#postProcessTimeline { border:0; background:#e9eef5; }"
        "QToolButton { border:1px solid transparent; padding:4px; background:transparent; }"
        "QToolButton:hover { background:#e0eaf6; border-color:#b8cde6; }"
        "QToolButton:checked { background:#dceafd; border-color:#9cbce1; }"));
    QTimer::singleShot(0, this, [this] {
        setStyleSheet(styleSheet() + QStringLiteral(
            "SARibbonBar { background:#e5e4e4; }"
            "SARibbonTabBar::tab { color:#222; border:0; margin:0; padding:0 12px; }"
            "SARibbonTabBar::tab:selected { color:#2563b6; background:#fff; border-bottom:2px solid #2563b6; }"
            "SARibbonStackedWidget { border:0; }"
            "SARibbonToolButton { border-radius:0; }"
            "QDockWidget::title { background:#e9eef5; padding:3px; color:#25364c; font:9pt 'Microsoft YaHei UI'; }"
            "QStatusBar { background:#e9eef5; color:#25364c; }"
            "QStatusBar,QStatusBar QLabel { font:9pt 'Microsoft YaHei UI'; }"
            "QStatusBar::item { border:0; }"));
    });
    updateDataSelection(datasetTree_->currentItem(), 0);
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1) updateDataSelection(datasetTree_->currentItem(), 0);
        if (index == 2) updateSelectionDetails();
    });
}

void PostProcessWorkbenchWindow::createActions() {
    auto *importAction = ribbonAction(this, commandIcon("import"), uiText(u"导入结果"), uiText(u"导入网格、结果或多时间步数据"), uiText(u"postProcessImportAction"));
    auto *exportAction = ribbonAction(this, commandIcon("export"), uiText(u"导出"), uiText(u"导出图层、几何、图像或动画"), uiText(u"postProcessExportAction"));
    auto *convertAction = ribbonAction(this, commandIcon("mesh"), uiText(u"网格转换"), uiText(u"结构网格转非结构网格"), uiText(u"postProcessConvertAction"));
    auto *excelAction = ribbonAction(this, commandIcon("analysis"), uiText(u"工况汇总"), uiText(u"汇总多工况数据并导出 Excel"), uiText(u"postProcessExcelAction"));
    auto *meshAction = ribbonAction(this, commandIcon("mesh"), uiText(u"网格 / 边线"), uiText(u"切换当前 Zone 的网格和边线"), uiText(u"postProcessMeshAction"));
    auto *contourAction = ribbonAction(this, commandIcon("contour"), uiText(u"新建云图"), uiText(u"将选中字段以云图方式添加到图层"), uiText(u"postProcessContourAction"));
    auto *colorAction = ribbonAction(this, commandIcon("palette"), uiText(u"色标范围"), uiText(u"配置字段的色标和值域"), uiText(u"postProcessColorAction"));
    auto *sliceAction = ribbonAction(this, commandIcon("slice"), uiText(u"添加切面"), uiText(u"在体网格中创建可编辑的切面"), uiText(u"postProcessSliceAction"));
    auto *isoAction = ribbonAction(this, commandIcon("iso"), uiText(u"等值面"), uiText(u"根据选中字段提取三维等值面"), uiText(u"postProcessIsoAction"));
    auto *streamAction = ribbonAction(this, commandIcon("stream"), uiText(u"流线"), uiText(u"从种子点生成流线或表面流线"), uiText(u"postProcessStreamAction"));
    auto *traceAction = ribbonAction(this, commandIcon("trace"), uiText(u"轨迹"), uiText(u"针对时间序结果生成粒子轨迹"), uiText(u"postProcessTraceAction"));
    auto *fitAction = ribbonAction(this, commandIcon("fit"), uiText(u"适配视图"), uiText(u"让可见图层适配当前视口"), uiText(u"postProcessFitAction"));
    auto *probeAction = ribbonAction(this, commandIcon("probe"), uiText(u"探针"), uiText(u"查询单元、节点的坐标和字段值"), uiText(u"postProcessProbeAction"));
    probeAction_ = probeAction;
    styleAction_ = ribbonAction(this, commandIcon("style"), uiText(u"对象样式…"), uiText(u"编辑选中对象的颜色、网格和可见性"), QStringLiteral("postProcessObjectStyleAction"));
    parameterAction_ = ribbonAction(this, commandIcon("parameters"), uiText(u"编辑参数…"), uiText(u"编辑选中对象的专属参数草稿"), QStringLiteral("postProcessParametersAction"));
    auto *deepSeekAction = ribbonAction(this, commandIcon("harness"), uiText(u"DeepSeek"), uiText(u"打开 DeepSeek Harness，基于当前选择组织后处理任务"), uiText(u"postProcessHarnessAction"));
    auto *geometryExportAction = ribbonAction(this, commandIcon("export"), uiText(u"特征几何"), uiText(u"导出切面、等值面等派生几何"), uiText(u"postProcessGeometryExportAction"));
    auto *animationExportAction = ribbonAction(this, commandIcon("trace"), uiText(u"时序动画"), uiText(u"导出当前时间序列动画"), uiText(u"postProcessAnimationExportAction"));
    auto *selectAction = ribbonAction(this, commandIcon("select"), uiText(u"选择"), uiText(u"选择 Zone、图层或派生对象"), uiText(u"postProcessSelectAction"));
    auto *boxSelectAction = ribbonAction(this, commandIcon("boxSelect"), uiText(u"框选"), uiText(u"框选多个可见对象"), uiText(u"postProcessBoxSelectAction"));
    auto *panAction = ribbonAction(this, commandIcon("pan"), uiText(u"平移"), uiText(u"平移当前视图"), uiText(u"postProcessPanAction"));
    auto *zoomAction = ribbonAction(this, commandIcon("zoom"), uiText(u"缩放"), uiText(u"缩放当前视图"), uiText(u"postProcessZoomAction"));
    auto *rotateAction = ribbonAction(this, commandIcon("rotate"), uiText(u"旋转"), uiText(u"旋转三维视图"), uiText(u"postProcessRotateAction"));
    auto *frontAction = ribbonAction(this, commandIcon("front"), uiText(u"前视"), uiText(u"切换至前视图"), uiText(u"postProcessFrontViewAction"));
    auto *rightAction = ribbonAction(this, commandIcon("right"), uiText(u"右视"), uiText(u"切换至右视图"), uiText(u"postProcessRightViewAction"));
    auto *topAction = ribbonAction(this, commandIcon("top"), uiText(u"俯视"), uiText(u"切换至俯视图"), uiText(u"postProcessTopViewAction"));
    auto *interactionModes = new QActionGroup(this);
    interactionModes->setExclusive(true);
    for (QAction *action : {selectAction, boxSelectAction, panAction, zoomAction, rotateAction, probeAction}) {
        action->setCheckable(true);
        interactionModes->addAction(action);
    }
    selectAction->setChecked(true);
    const QList<QString> analysisNames = {uiText(u"收敛曲线"), uiText(u"六分量汇总"), uiText(u"公式与舵效"), uiText(u"系列线与图表"), uiText(u"AI 气动预测")};
    QList<QAction *> analysisActions;
    for (const QString &name : analysisNames) {
        analysisActions.append(ribbonAction(this, commandIcon(QStringList({"curve", "compare", "formula", "chart", "harness"}).at(analysisActions.size())), name,
                                            uiText(u"打开") + name + uiText(u"工作流程"),
                                            uiText(u"postProcessAnalysisAction")));
    }

    // Use the same SARibbon hierarchy as MainWindow.  Categories group work
    // by intent; the data/scene workspace below remains the post-process UI.
    auto *dataCategory = ribbonBar()->addCategoryPage(uiText(u"数据"));
    dataCategory->setObjectName(uiText(u"postProcessDataCategory"));
    auto *dataSourcePanel = dataCategory->addPanel(uiText(u"结果数据"));
    dataSourcePanel->addLargeAction(importAction); dataSourcePanel->addSmallAction(exportAction);
    auto *dataToolsPanel = dataCategory->addPanel(uiText(u"数据处理"));
    dataToolsPanel->addLargeAction(convertAction); dataToolsPanel->addSmallAction(excelAction);

    auto *displayCategory = ribbonBar()->addCategoryPage(uiText(u"显示"));
    displayCategory->setObjectName(uiText(u"postProcessDisplayCategory"));
    auto *interactionPanel = displayCategory->addPanel(uiText(u"交互"));
    interactionPanel->addLargeAction(selectAction); interactionPanel->addSmallAction(boxSelectAction);
    interactionPanel->addSmallAction(panAction); interactionPanel->addSmallAction(zoomAction); interactionPanel->addSmallAction(rotateAction);
    auto *layerPanel = displayCategory->addPanel(uiText(u"图层"));
    layerPanel->addLargeAction(meshAction); layerPanel->addLargeAction(contourAction); layerPanel->addSmallAction(colorAction); layerPanel->addSmallAction(styleAction_);
    auto *cameraPanel = displayCategory->addPanel(uiText(u"视图"));
    cameraPanel->addLargeAction(fitAction); cameraPanel->addSmallAction(frontAction); cameraPanel->addSmallAction(rightAction); cameraPanel->addSmallAction(topAction);

    auto *featureCategory = ribbonBar()->addCategoryPage(uiText(u"特征"));
    featureCategory->setObjectName(uiText(u"postProcessFeatureCategory"));
    auto *spatialPanel = featureCategory->addPanel(uiText(u"空间特征"));
    spatialPanel->addLargeAction(sliceAction); spatialPanel->addSmallAction(isoAction);
    spatialPanel->addSmallAction(streamAction); spatialPanel->addSmallAction(traceAction);
    spatialPanel->addSmallAction(parameterAction_);

    auto *analysisCategory = ribbonBar()->addCategoryPage(uiText(u"分析"));
    analysisCategory->setObjectName(uiText(u"postProcessAnalysisCategory"));
    auto *analysisPanel = analysisCategory->addPanel(uiText(u"结果分析"));
    for (QAction *action : analysisActions) analysisPanel->addLargeAction(action);

    auto *toolsCategory = ribbonBar()->addCategoryPage(uiText(u"工具"));
    toolsCategory->setObjectName(uiText(u"postProcessToolsCategory"));
    auto *queryPanel = toolsCategory->addPanel(uiText(u"查询与导出"));
    queryPanel->addLargeAction(probeAction); queryPanel->addSmallAction(geometryExportAction); queryPanel->addSmallAction(animationExportAction);
    auto *assistantPanel = toolsCategory->addPanel(uiText(u"智能辅助"));
    assistantPanel->addLargeAction(deepSeekAction);

    // The landing page carries the former quick actions in the same ribbon
    // row as data operations. Reuse QActions so checked/enabled state stays in
    // sync with their specialist categories; never add a second toolbar.
    auto *quickInteraction = dataCategory->addPanel(uiText(u"交互"));
    quickInteraction->addLargeAction(selectAction);
    for (QAction *action : {boxSelectAction, panAction, zoomAction, rotateAction}) quickInteraction->addSmallAction(action);
    auto *quickLayers = dataCategory->addPanel(uiText(u"显示"));
    quickLayers->addLargeAction(contourAction);
    quickLayers->addSmallAction(meshAction); quickLayers->addSmallAction(colorAction); quickLayers->addSmallAction(styleAction_);
    auto *quickFeatures = dataCategory->addPanel(uiText(u"空间特征"));
    quickFeatures->addLargeAction(sliceAction);
    for (QAction *action : {isoAction, streamAction, traceAction}) quickFeatures->addSmallAction(action);
    auto *quickCamera = dataCategory->addPanel(uiText(u"视图"));
    quickCamera->addLargeAction(fitAction);
    for (QAction *action : {frontAction, rightAction, topAction}) quickCamera->addSmallAction(action);
    auto *quickQuery = dataCategory->addPanel(uiText(u"查询与辅助"));
    quickQuery->addSmallAction(probeAction); quickQuery->addSmallAction(deepSeekAction);

    auto *panelsMenu = new QMenu(uiText(u"面板"), this);
    panelsMenu->setObjectName(QStringLiteral("postProcessPanelsMenu"));
    auto *resultsAction = ribbonAction(this, commandIcon("probe"), uiText(u"探针结果"), uiText(u"查看已有取点结果，不改变鼠标模式"), QStringLiteral("postProcessProbeResultsAction"));
    auto *tasksAction = ribbonAction(this, commandIcon("analysis"), uiText(u"任务与消息"), uiText(u"查看任务事件与操作消息"), QStringLiteral("postProcessTasksAction"));
    panelsMenu->addAction(resultsAction); panelsMenu->addAction(tasksAction);
    auto *panelsAction = ribbonAction(this, commandIcon("dock"), uiText(u"面板"), uiText(u"按需打开结果与任务面板"), QStringLiteral("postProcessPanelsAction"));
    panelsAction->setMenu(panelsMenu);
    quickQuery->addSmallAction(panelsAction);
    auto *panelGroup = toolsCategory->addPanel(uiText(u"面板"));
    panelGroup->addSmallAction(resultsAction); panelGroup->addSmallAction(tasksAction);
    connect(resultsAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::showProbePanel);
    connect(tasksAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::showTaskPanel);
    connect(styleAction_, &QAction::triggered, this, &PostProcessWorkbenchWindow::openObjectStyle);
    connect(parameterAction_, &QAction::triggered, this, &PostProcessWorkbenchWindow::openObjectParameters);

    auto *interactionHint = new QLabel(uiText(u"选择模式 · 左键选择 / 中键平移 / 滚轮缩放"), this);
    interactionHint->setObjectName(uiText(u"postProcessInteractionHint"));
    statusBar()->addPermanentWidget(interactionHint, 1);
    featureActions_ = {meshAction, contourAction, sliceAction, isoAction, streamAction, traceAction};
    const auto command = [this](const QString &key, const QString &message) { emit postProcessCommandRequested(key); appendMessage(message); };
    connect(importAction, &QAction::triggered, this, [command] { command(uiText(u"importResults"), uiText(u"已打开导入结果流程；结果读取服务接入后将填充数据集树。")); });
    connect(exportAction, &QAction::triggered, this, [command] { command(uiText(u"export"), uiText(u"已提交导出请求；请在图层页选择导出对象。")); });
    const auto setInteractionMode = [this, interactionHint](const QString &mode, const QString &hint) {
        interactionHint->setText(hint);
        emit postProcessCommandRequested(QStringLiteral("interactionMode:") + mode);
        appendMessage(uiText(u"已切换至") + mode + uiText(u"模式。"));
    };
    connect(selectAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(uiText(u"选择"), uiText(u"选择模式 · 左键选择 / 中键平移 / 滚轮缩放")); });
    connect(boxSelectAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(uiText(u"框选"), uiText(u"框选模式 · 在视口拖曳以选择对象")); });
    connect(panAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(uiText(u"平移"), uiText(u"平移模式 · 在视口拖曳以移动视图")); });
    connect(zoomAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(uiText(u"缩放"), uiText(u"缩放模式 · 拖曳或滚轮缩放视图")); });
    connect(rotateAction, &QAction::triggered, this, [setInteractionMode] { setInteractionMode(uiText(u"旋转"), uiText(u"旋转模式 · 拖曳以旋转三维视图")); });
    connect(frontAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"front")); appendMessage(uiText(u"已切换至前视图。")); });
    connect(rightAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"right")); appendMessage(uiText(u"已切换至右视图。")); });
    connect(topAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"top")); appendMessage(uiText(u"已切换至俯视图。")); });
    connect(convertAction, &QAction::triggered, this, [command] { command(uiText(u"convertStructuredMesh"), uiText(u"已打开结构网格转非结构网格流程。")); }); connect(excelAction, &QAction::triggered, this, [command] { command(uiText(u"aggregateExcelCases"), uiText(u"已打开 Excel 工况汇总流程。")); }); connect(geometryExportAction, &QAction::triggered, this, [command] { command(uiText(u"exportFeatureGeometry"), uiText(u"已打开特征几何导出流程。")); }); connect(animationExportAction, &QAction::triggered, this, [command] { command(uiText(u"exportAnimation"), uiText(u"已打开时间序列动画导出流程。")); });
    connect(meshAction, &QAction::triggered, this, [command] { command(uiText(u"toggleMesh"), uiText(u"已切换当前 Zone 的网格显示。")); });
    connect(colorAction, &QAction::triggered, this, [this] {
        auto *fields = datasetTree_->topLevelItem(1);
        if (selectedKind_ != "field" && fields && fields->childCount()) datasetTree_->setCurrentItem(fields->child(0));
        openObjectParameters();
    });
    const auto contourFn = [this] { addDerivedLayer(uiText(u"contour"), uiText(u"压力云图"), uiText(u"已将“压力”以云图添加到图层。")); }; connect(contourAction, &QAction::triggered, this, contourFn);
    const auto sliceFn = [this] { addDerivedLayer(uiText(u"slice"), uiText(u"切面 1（X = 0.0）"), uiText(u"已创建切面；可通过“编辑参数…”修改位置与显示字段。")); }; connect(sliceAction, &QAction::triggered, this, sliceFn);
    const auto isoFn = [this] { addDerivedLayer(uiText(u"iso"), uiText(u"等值面 1（压力）"), uiText(u"已创建等值面；请设置等值以便后续服务执行。")); }; connect(isoAction, &QAction::triggered, this, isoFn);
    const auto streamFn = [this] { addDerivedLayer(uiText(u"stream"), uiText(u"流线 1"), uiText(u"已创建流线对象；请通过“编辑参数…”设置种子和积分方向。")); }; connect(streamAction, &QAction::triggered, this, streamFn);
    const auto traceFn = [this] { addDerivedLayer(uiText(u"trace"), uiText(u"粒子轨迹 1"), uiText(u"已创建轨迹对象；时序数据接入后可运行追踪。")); }; connect(traceAction, &QAction::triggered, this, traceFn);
    connect(fitAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"reset")); appendMessage(uiText(u"视图已适配所有可见图层。")); }); connect(probeAction, &QAction::triggered, this, [command] { command(uiText(u"probe"), uiText(u"已进入探针查询模式：请在视口点选单元或节点。")); });
    connect(deepSeekAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::openDeepSeekHarness);
    connect(probeAction, &QAction::triggered, this, [interactionHint] {
        interactionHint->setText(uiText(u"探针模式 · 点击数据位置取点；取点结果将自动显示"));
    });
    for (QAction *action : analysisActions) connect(action, &QAction::triggered, this, [command, action] { command(uiText(u"openAnalysis"), uiText(u"已打开“%1”工作流程入口。").arg(action->text())); });
}

void PostProcessWorkbenchWindow::openDeepSeekHarness() {
    if (!deepSeekHarnessDock_) {
        deepSeekHarnessDock_ = new QDockWidget(uiText(u"DeepSeek Harness"), this);
        installDockTitle(deepSeekHarnessDock_);
        deepSeekHarnessDock_->setObjectName(uiText(u"deepSeekHarnessDock"));
        auto *panel = new QWidget(deepSeekHarnessDock_);
        auto *layout = new QVBoxLayout(panel);
        auto *hint = new QLabel(uiText(u"根据当前选择的 Zone、字段、图层或时间步组织后处理任务。"), panel);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto *scope = new QComboBox(panel);
        scope->addItems({uiText(u"当前选择"), uiText(u"当前视口"), uiText(u"当前结果集")});
        layout->addWidget(scope);
        auto *prompt = new QTextEdit(panel);
        prompt->setObjectName(uiText(u"deepSeekHarnessPrompt"));
        prompt->setPlaceholderText(uiText(u"例如：对翼面 Zone 的压力场生成三个切面，并导出对比图。"));
        prompt->setMinimumHeight(180);
        layout->addWidget(prompt);
        auto *run = new QPushButton(uiText(u"提交后处理任务"), panel);
        run->setObjectName(uiText(u"primaryAction"));
        layout->addWidget(run);
        auto *state = new QLabel(uiText(u"Harness 服务尚未接入；此入口会发出任务请求并保留当前上下文。"), panel);
        state->setObjectName(uiText(u"mutedText")); state->setWordWrap(true);
        layout->addWidget(state); layout->addStretch();
        panel->setStyleSheet(centralWidget()->styleSheet());
        deepSeekHarnessDock_->setWidget(scrollPanel(panel));
        addDockWidget(Qt::RightDockWidgetArea, deepSeekHarnessDock_);
        connect(run, &QPushButton::clicked, this, [this, prompt, scope] {
            emit postProcessCommandRequested(uiText(u"openDeepSeekHarness"));
            appendMessage(prompt->toPlainText().trimmed().isEmpty()
                ? uiText(u"DeepSeek Harness 已打开；请输入后处理任务。")
                : uiText(u"已向 DeepSeek Harness 提交当前上下文与任务描述。"));
        });
    }
    deepSeekHarnessDock_->show();
    deepSeekHarnessDock_->raise();
    emit postProcessCommandRequested(uiText(u"openDeepSeekHarness"));
    appendMessage(uiText(u"DeepSeek Harness 入口已打开。"));
}

QWidget *PostProcessWorkbenchWindow::createDataPanel() {
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8); auto *row = new QHBoxLayout; row->addWidget(sectionLabel(uiText(u"当前结果集"), panel)); row->addStretch(); auto *import = new QPushButton(uiText(u"导入"), panel); import->setIcon(commandIcon("import")); import->setObjectName(uiText(u"primaryAction")); row->addWidget(import); layout->addLayout(row);
    auto *card = new QFrame(panel); card->setObjectName(uiText(u"dataCard")); auto *cardLayout = new QVBoxLayout(card); sessionContext_ = new QLabel(uiText(u"翼身组合气动算例 / 结果集"), card); sessionContext_->setWordWrap(true); sessionContext_->setStyleSheet(uiText(u"font-weight:600;color:#25364c;")); auto *description = new QLabel(uiText(u"选择 Zone、边界或字段后，工具栏仅点亮当前数据支持的功能。"), card); description->setObjectName(uiText(u"mutedText")); description->setWordWrap(true); cardLayout->addWidget(sessionContext_); cardLayout->addWidget(description); layout->addWidget(card);
    selectionSummary_ = new QLabel(uiText(u"当前选择：流场体网格"), panel); selectionSummary_->setWordWrap(true); selectionSummary_->setObjectName(uiText(u"mutedText")); layout->addWidget(selectionSummary_); datasetTree_ = new QTreeWidget(panel); datasetTree_->setObjectName(uiText(u"postProcessDatasetTree")); datasetTree_->setHeaderHidden(true); datasetTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *zones = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"网格与 Zone"))); zones->setData(0, Qt::UserRole, "group"); auto *volume = new QTreeWidgetItem(zones, QStringList(uiText(u"流场体网格"))); volume->setData(0, Qt::UserRole, "volume"); for (const QString &text : {uiText(u"翼面 Zone"), uiText(u"机身壁面")}) { auto *item = new QTreeWidgetItem(zones, QStringList(uiText(u"") + text)); item->setData(0, Qt::UserRole, "surface"); }
    auto *fields = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"物理字段"))); fields->setData(0, Qt::UserRole, "group"); for (const QString &text : {uiText(u"压力 [Pa]"), uiText(u"速度大小 [m/s]"), uiText(u"温度 [K]"), uiText(u"马赫数 [-]")}) { auto *item = new QTreeWidgetItem(fields, QStringList(uiText(u"") + text)); item->setData(0, Qt::UserRole, "field"); }
    auto *time = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"时间序列"))); time->setData(0, Qt::UserRole, "time"); for (const QString &text : {uiText(u"t = 0.0 s"), uiText(u"t = 0.5 s"), uiText(u"t = 1.0 s")}) { auto *item = new QTreeWidgetItem(time, QStringList(text)); item->setData(0, Qt::UserRole, "time"); }
    auto *monitoring = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"监控数据"))); monitoring->setData(0, Qt::UserRole, "group"); auto *curve = new QTreeWidgetItem(monitoring, QStringList(uiText(u"阻力系数监控曲线"))); curve->setData(0, Qt::UserRole, "curve"); for (int i = 0; i < datasetTree_->topLevelItemCount(); ++i) {
        auto *group = datasetTree_->topLevelItem(i);
        const QString key = i == 0 ? "mesh" : i == 1 ? "field" : i == 2 ? "trace" : "analysis";
        group->setIcon(0, commandIcon(key));
        group->setData(0, Qt::UserRole + 1, QStringLiteral("group-%1").arg(i));
        for (int j = 0; j < group->childCount(); ++j) {
            group->child(j)->setIcon(0, commandIcon(key));
            group->child(j)->setData(0, Qt::UserRole + 1, QStringLiteral("data-%1-%2").arg(i).arg(j));
        }
    }
    connect(datasetTree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int column) {
        updateDataSelection(item, column); openSelectedSettings();
    });
    datasetTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(datasetTree_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        auto *item = datasetTree_->itemAt(point); if (!item) return;
        datasetTree_->setCurrentItem(item); updateDataSelection(item,0);
        QMenu menu(this); menu.addAction(styleAction_); menu.addAction(parameterAction_);
        menu.exec(datasetTree_->viewport()->mapToGlobal(point));
    });
    datasetTree_->expandAll(); datasetTree_->setCurrentItem(volume); layout->addWidget(datasetTree_, 1); auto *hint = new QLabel(uiText(u"数据集中的 Zone、字段与时间步由读取服务提供；派生对象在“对象”页管理。"), panel); hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint); connect(datasetTree_, &QTreeWidget::itemClicked, this, &PostProcessWorkbenchWindow::updateDataSelection); connect(datasetTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) { updateDataSelection(current, 0); }); connect(import, &QPushButton::clicked, this, [this] { emit postProcessCommandRequested(uiText(u"importResults")); appendMessage(uiText(u"已打开结果导入流程。")); }); return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(sectionLabel(uiText(u"Frame 显示控制"), panel));
    auto *hint = new QLabel(uiText(u"这些开关作用于当前 Frame 的所有 Zone；单个 Zone 的样式请通过“对象样式…”设置。"), panel);
    hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint);

    auto *layers = new QGroupBox(uiText(u"全局图层（当前 Frame）"), panel);
    auto *layerLayout = new QVBoxLayout(layers);
    const QList<QPair<QString, bool>> layerItems = {
        {uiText(u"网格"), true}, {uiText(u"云图"), true}, {uiText(u"表面"), true},
        {uiText(u"边线"), true}, {uiText(u"矢量"), false}, {uiText(u"散点"), false}};
    for (const auto &entry : layerItems) {
        auto *toggle = new QCheckBox(entry.first, layers); toggle->setChecked(entry.second); layerLayout->addWidget(toggle);
        connect(toggle, &QCheckBox::toggled, this, [this, entry](bool enabled) {
            appendMessage(uiText(u"已%1当前视口的“%2”显示层。").arg(enabled ? uiText(u"显示") : uiText(u"隐藏"), entry.first));
        });
    }
    layout->addWidget(layers);
    auto *styleButton = new QToolButton(panel);
    styleButton->setDefaultAction(styleAction_); styleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    layout->addWidget(styleButton);

    auto *scope = new QFrame(panel); scope->setObjectName(uiText(u"dataCard")); auto *scopeLayout = new QVBoxLayout(scope);
    scopeLayout->addWidget(sectionLabel(uiText(u"作用域"), scope));
    auto *scopeHint = new QLabel(uiText(u"当前 Frame · 三维笛卡尔 · 所有 Zone"), scope); scopeHint->setWordWrap(true); scopeHint->setObjectName(uiText(u"mutedText")); scopeLayout->addWidget(scopeHint);
    layout->addWidget(scope); layout->addStretch();
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createObjectPanel() {
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(sectionLabel(uiText(u"对象与图层"), panel));
    auto *hint = new QLabel(uiText(u"对象属于当前 Frame。双击对象编辑参数，或通过“对象样式…”编辑外观；显隐和删除只影响选中对象。"), panel);
    hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint);
    auto *row = new QHBoxLayout;
    row->addWidget(sectionLabel(uiText(u"派生对象"), panel)); row->addStretch();
    auto *add = new QPushButton(uiText(u"新建"), panel); add->setIcon(commandIcon("add")); row->addWidget(add); layout->addLayout(row);
    sceneList_ = new QListWidget(panel); sceneList_->setObjectName(uiText(u"sceneLayerList"));
    sceneList_->addItems({uiText(u"切面 1（X = 0.0）  可见"), uiText(u"等值面 1（压力） 可见"), uiText(u"流线 1           可见")});
    for (int i = 0; i < sceneList_->count(); ++i) {
        auto *item = sceneList_->item(i);
        const QString kind = i == 0 ? "slice" : i == 1 ? "iso" : "stream";
        item->setIcon(commandIcon(kind)); item->setData(Qt::UserRole, kind);
        item->setData(Qt::UserRole+1, QStringLiteral("object-%1").arg(nextObjectId_++));
        item->setData(Qt::UserRole+2, item->text().left(item->text().lastIndexOf(uiText(u"可见"))).trimmed());
    }
    sceneList_->setCurrentRow(0); layout->addWidget(sceneList_, 1);
    auto *actions = new QHBoxLayout; auto *visible = new QPushButton(uiText(u"显隐"), panel); auto *remove = new QPushButton(uiText(u"删除"), panel); visible->setIcon(commandIcon("visible")); remove->setIcon(commandIcon("delete")); actions->addWidget(visible); actions->addWidget(remove); layout->addLayout(actions);
    connect(add, &QPushButton::clicked, this, [this] { addDerivedLayer(uiText(u"layer"), uiText(u"新派生对象"), uiText(u"已创建派生对象，请通过“编辑参数…”配置。")); });
    connect(sceneList_, &QListWidget::itemSelectionChanged, this, &PostProcessWorkbenchWindow::updateSelectionDetails);
    connect(sceneList_, &QListWidget::itemClicked, this, [this] { updateSelectionDetails(); });
    auto *edit = new QToolButton(panel); edit->setDefaultAction(parameterAction_);
    edit->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); layout->addWidget(edit);
    connect(sceneList_, &QListWidget::itemDoubleClicked, this, [this] { updateSelectionDetails(); openObjectParameters(); });
    sceneList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sceneList_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        auto *item = sceneList_->itemAt(point); if (!item) return;
        sceneList_->setCurrentItem(item); updateSelectionDetails();
        QMenu menu(this); menu.addAction(styleAction_); menu.addAction(parameterAction_);
        menu.exec(sceneList_->viewport()->mapToGlobal(point));
    });
    connect(visible, &QPushButton::clicked, this, [this] {
        auto *item = sceneList_->currentItem(); if (!item) return;
        const bool isVisible = item->text().endsWith(uiText(u"可见"));
        item->setText(item->data(Qt::UserRole+2).toString() + (isVisible ? uiText(u"  隐藏") : uiText(u"  可见")));
        styleDrafts_[item->data(Qt::UserRole+1).toString()]["visible"] = !isVisible;
        appendMessage(isVisible ? uiText(u"已隐藏选中派生对象。") : uiText(u"已显示选中派生对象。"));
    });
    connect(remove, &QPushButton::clicked, this, [this] { if (sceneList_->currentItem()) { const QString id = sceneList_->currentItem()->data(Qt::UserRole+1).toString(); styleDrafts_.remove(id); parameterDrafts_.remove(id); delete sceneList_->takeItem(sceneList_->currentRow()); updateSelectionDetails(); appendMessage(uiText(u"已移除选中派生对象。")); } });
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewportPanel() {
    auto *frame = new QFrame(this); frame->setObjectName(uiText(u"viewportFrame"));
    auto *layout = new QVBoxLayout(frame); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *header = new QFrame(frame); header->setObjectName(uiText(u"viewportHeader")); auto *headerLayout = new QHBoxLayout(header); headerLayout->setContentsMargins(10, 4, 10, 4);
    viewportTitle_ = new QLabel(uiText(u"Frame 1 · 三维笛卡尔"), header); viewportTitle_->setObjectName(uiText(u"viewportTitle"));
    viewportHint_ = new QLabel(uiText(u"翼身组合 · 压力 [Pa] · t = 0.0 s"), header); viewportHint_->setObjectName(uiText(u"mutedText"));
    auto *orientation = new QLabel(uiText(u"Z ↑   Y →"), header); orientation->setObjectName(uiText(u"viewportOrientation"));
    auto *caption = new QVBoxLayout;
    viewportTitle_->setWordWrap(true); viewportHint_->setWordWrap(true);
    caption->addWidget(viewportTitle_); caption->addWidget(viewportHint_);
    headerLayout->addLayout(caption, 1); headerLayout->addWidget(orientation);
    layout->addWidget(header);
    auto *viewportHost = new QFrame(frame); viewportHost->setObjectName(uiText(u"postProcessViewportHost")); viewportLayout_ = new QVBoxLayout(viewportHost); viewportLayout_->setContentsMargins(0, 0, 0, 0);
    emptyViewport_ = new ResultPreviewWidget(viewportHost); emptyViewport_->setObjectName(uiText(u"postProcessResultPreview")); viewportLayout_->addWidget(emptyViewport_);
    layout->addWidget(viewportHost, 1); return frame;
}

QWidget *PostProcessWorkbenchWindow::createTimeline() {
    auto *frame = new QFrame(this); frame->setObjectName(uiText(u"postProcessTimeline"));
    auto *layout = new QHBoxLayout(frame); layout->setContentsMargins(8,5,8,5);
    layout->addWidget(new QLabel(uiText(u"时间"), frame));
    const auto addStepButton = [this, frame, layout](const QString &text, const QString &name, const std::function<int(int)> &next) {
        auto *button = new QToolButton(frame); button->setObjectName(name);
        const QString key = name.contains("First") ? "first" : name.contains("Previous") ? "previous" : name.contains("Next") ? "next" : "last";
        button->setIcon(commandIcon(key)); button->setIconSize(QSize(20,20));
        button->setToolTip(text); button->setAccessibleName(text); layout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, next] { timeSlider_->setValue(next(timeSlider_->value())); });
    };
    addStepButton(uiText(u"首帧"), uiText(u"postProcessFirstFrame"), [](int) { return 0; });
    addStepButton(uiText(u"上一帧"), uiText(u"postProcessPreviousFrame"), [](int value) { return qMax(0, value - 1); });
    addStepButton(uiText(u"下一帧"), uiText(u"postProcessNextFrame"), [](int value) { return qMin(2, value + 1); });
    addStepButton(uiText(u"末帧"), uiText(u"postProcessLastFrame"), [](int) { return 2; });
    timeSlider_=new QSlider(Qt::Horizontal,frame); timeSlider_->setObjectName(uiText(u"postProcessTimeSlider")); timeSlider_->setRange(0,2); layout->addWidget(timeSlider_,1);
    timeLabel_=new QLabel(uiText(u"Frame 1 / 3 · t = 0.0 s"),frame); layout->addWidget(timeLabel_);
    connect(timeSlider_, &QSlider::valueChanged, this, [this](int value) {
        timeLabel_->setText(uiText(u"Frame %1 / 3 · t = %2 s").arg(value + 1).arg(value * .5, 0, 'f', 1));
        if (viewportHint_) viewportHint_->setText(uiText(u"翼身组合 · 压力 [Pa] · t = %1 s").arg(value * .5, 0, 'f', 1));
        appendMessage(uiText(u"当前 Frame 已定位至 t = %1 s。").arg(value * .5, 0, 'f', 1));
    }); return frame;
}

void PostProcessWorkbenchWindow::setFeatureActionsEnabled(bool mesh, bool contour, bool slice, bool iso, bool streamline) { const bool states[]={mesh,contour,slice,iso,streamline,streamline}; for(int i=0;i<featureActions_.size();++i){featureActions_.at(i)->setEnabled(states[i]);featureActions_.at(i)->setToolTip(states[i]?featureActions_.at(i)->property("availableToolTip").toString():uiText(u"当前选中数据不支持此功能。请选择体网格或相关字段。"));} }
void PostProcessWorkbenchWindow::updateDataSelection(QTreeWidgetItem *item, int) {
    if (!item) return;
    selectedKind_ = item->data(0, Qt::UserRole).toString();
    selectedId_ = item->data(0, Qt::UserRole+1).toString();
    selectedName_ = item->text(0).trimmed();
    if (selectionSummary_) selectionSummary_->setText(uiText(u"当前选择：") + selectedName_);
    if (selectedKind_ == "time" && timeSlider_) {
        const int index = selectedName_.contains("1.0") ? 2 : selectedName_.contains("0.5") ? 1 : 0;
        timeSlider_->setValue(index);
    }
    updateSelectionActions();
}

void PostProcessWorkbenchWindow::updateSelectionActions() {
    const bool zone = selectedKind_ == "volume" || selectedKind_ == "surface";
    const bool field = selectedKind_ == "field";
    const bool derived = selectedKind_ == "slice" || selectedKind_ == "iso" || selectedKind_ == "stream" || selectedKind_ == "trace" || selectedKind_ == "contour" || selectedKind_ == "layer";
    styleAction_->setEnabled(zone || derived || selectedKind_ == "curve");
    parameterAction_->setEnabled(derived || field || selectedKind_ == "curve");
    setFeatureActionsEnabled(zone || field, zone || field, selectedKind_ == "volume" || field,
                             selectedKind_ == "volume" || field, zone || field);
}

void PostProcessWorkbenchWindow::addDerivedLayer(const QString &kind, const QString &name, const QString &message) {
    emit postProcessCommandRequested(kind);
    auto *item = new QListWidgetItem(commandIcon(kind == "layer" ? "add" : kind), name + uiText(u"  可见"), sceneList_);
    item->setData(Qt::UserRole, kind);
    item->setData(Qt::UserRole+1, QStringLiteral("object-%1").arg(nextObjectId_++));
    item->setData(Qt::UserRole+2, name);
    sceneList_->setCurrentItem(item);
    updateSelectionDetails();
    appendMessage(message);
}
bool PostProcessWorkbenchWindow::insertWidget(WidgetSlot slot,QWidget *widget){if(slot!=WidgetSlot::Viewport||!widget||!viewportLayout_)return false;if(emptyViewport_){viewportLayout_->removeWidget(emptyViewport_);emptyViewport_->deleteLater();emptyViewport_=nullptr;}if(viewportLayout_->count()!=0)return false;viewportLayout_->addWidget(widget);return true;}
void PostProcessWorkbenchWindow::bindServices(const ServiceBindings &services){if(bound_)return;services_=services;bound_=true;if(services_.tasks)connect(services_.tasks,&ITaskService::taskEventOccurred,this,[this](const TaskEvent &event){appendMessage(event.message);},Qt::QueuedConnection);}
void PostProcessWorkbenchWindow::appendMessage(const QString &message){events_->append(message.toHtmlEscaped());statusBar()->showMessage(message,5000);}
void PostProcessWorkbenchWindow::updateSelectionDetails() {
    auto *item = sceneList_->currentItem();
    selectedId_ = item ? item->data(Qt::UserRole+1).toString() : QString();
    selectedKind_ = item ? item->data(Qt::UserRole).toString() : QString();
    selectedName_ = item ? item->data(Qt::UserRole+2).toString() : QString();
    updateSelectionActions();
}

void PostProcessWorkbenchWindow::openSelectedSettings() {
    if (selectedKind_ == "volume" || selectedKind_ == "surface") openObjectStyle();
    else if (parameterAction_->isEnabled()) openObjectParameters();
}

void PostProcessWorkbenchWindow::openObjectStyle() {
    if (!styleAction_->isEnabled()) return;
    if (styleDialog_) { styleDialog_->raise(); styleDialog_->activateWindow(); return; }
    const QString id = selectedId_;
    const QString name = selectedName_;
    const QVariantMap draft = styleDrafts_.value(id);
    auto *dialog = new QDialog(this);
    styleDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setObjectName(QStringLiteral("postProcessObjectStyleDialog"));
    dialog->setWindowTitle(uiText(u"对象样式 · ") + name);
    dialog->setStyleSheet(centralWidget()->styleSheet());
    dialog->resize(480, 340);
    auto *layout = new QVBoxLayout(dialog);
    auto *context = new QLabel(uiText(u"当前对象：") + name, dialog);
    context->setWordWrap(true); layout->addWidget(context);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *visible = new QCheckBox(uiText(u"在当前 Frame 中显示"), dialog);
    visible->setObjectName(QStringLiteral("objectStyleVisible"));
    visible->setChecked(draft.value("visible", true).toBool());
    auto *mesh = new QCheckBox(uiText(u"显示网格 / 边线"), dialog);
    mesh->setObjectName(QStringLiteral("objectStyleMesh"));
    mesh->setChecked(draft.value("mesh", true).toBool());
    mesh->setEnabled(selectedKind_ != "curve");
    auto *color = new QComboBox(dialog);
    color->setObjectName(QStringLiteral("objectStyleColor"));
    color->addItems({uiText(u"继承图层"), uiText(u"蓝色"), uiText(u"青色"), uiText(u"橙色"), uiText(u"灰色")});
    color->setCurrentIndex(draft.value("color", 0).toInt());
    auto *opacity = new QSpinBox(dialog);
    opacity->setObjectName(QStringLiteral("objectStyleOpacity"));
    opacity->setRange(0,100); opacity->setSuffix(" %"); opacity->setValue(draft.value("opacity",100).toInt());
    form->addRow(visible); form->addRow(mesh);
    form->addRow(uiText(u"颜色"), color); form->addRow(uiText(u"不透明度"), opacity);
    layout->addLayout(form);
    auto *hint = new QLabel(uiText(u"当前保存的是此对象的界面草稿。场景服务接入后才会更新实际渲染。"), dialog);
    hint->setObjectName(QStringLiteral("mutedText")); hint->setWordWrap(true);
    layout->addWidget(hint); layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"保存草稿"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"取消")); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, id, name, visible, mesh, color, opacity] {
        styleDrafts_[id] = {{"visible", visible->isChecked()}, {"mesh", mesh->isChecked()},
                            {"color", color->currentIndex()}, {"opacity", opacity->value()}};
        for (int i = 0; i < sceneList_->count(); ++i) {
            auto *item = sceneList_->item(i);
            if (item->data(Qt::UserRole+1).toString() == id)
                item->setText(item->data(Qt::UserRole+2).toString() + (visible->isChecked() ? uiText(u"  可见") : uiText(u"  隐藏")));
        }
        appendMessage(uiText(u"已保存“%1”的样式草稿；实际渲染尚未接入。").arg(name));
        dialog->accept();
    });
    dialog->show();
}

void PostProcessWorkbenchWindow::openObjectParameters() {
    if (!parameterAction_->isEnabled()) return;
    if (parameterDialog_) { parameterDialog_->raise(); parameterDialog_->activateWindow(); return; }
    const QString id = selectedId_;
    const QString kind = selectedKind_;
    const QString name = selectedName_;
    const QVariantMap draft = parameterDrafts_.value(id);
    auto *dialog = new QDialog(this);
    parameterDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setObjectName(QStringLiteral("postProcessParameterDialog"));
    const QString title = kind == "slice" ? uiText(u"编辑切面") : kind == "iso" ? uiText(u"编辑等值面")
        : kind == "stream" ? uiText(u"编辑流线") : kind == "trace" ? uiText(u"编辑轨迹")
        : kind == "curve" ? uiText(u"曲线设置") : (kind == "field" || kind == "contour") ? uiText(u"字段与色标") : uiText(u"对象参数");
    dialog->setWindowTitle(title + uiText(u" · ") + name);
    dialog->setStyleSheet(centralWidget()->styleSheet()); dialog->resize(520,420);
    auto *layout = new QVBoxLayout(dialog);
    auto *context = new QLabel(uiText(u"当前对象：") + name, dialog); context->setWordWrap(true); layout->addWidget(context);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    QMap<QString, QDoubleSpinBox *> numbers;
    QMap<QString, QComboBox *> choices;
    const auto number = [&](const QString &key, const QString &label, double value) {
        auto *input = new QDoubleSpinBox(dialog); input->setObjectName(key);
        input->setDecimals(4); input->setRange(-1.e9,1.e9); input->setValue(draft.value(key,value).toDouble());
        form->addRow(label,input); numbers.insert(key,input);
    };
    const auto choice = [&](const QString &key, const QString &label, const QStringList &items) {
        auto *input = new QComboBox(dialog); input->setObjectName(key); input->addItems(items);
        input->setCurrentIndex(draft.value(key,0).toInt()); form->addRow(label,input); choices.insert(key,input);
    };
    if (kind == "slice") {
        choice("normal",uiText(u"法向"), {"X", "Y", "Z"});
        number("offset",uiText(u"位置 / m"),0.0);
    } else if (kind == "iso") {
        number("isoValue",uiText(u"等值"),0.0);
    } else if (kind == "stream" || kind == "trace") {
        number("seedX",uiText(u"种子 X / m"),0.0);
        number("seedY",uiText(u"种子 Y / m"),0.0);
        number("seedZ",uiText(u"种子 Z / m"),0.0);
        choice("direction",uiText(u"积分方向"), {uiText(u"向前"),uiText(u"向后"),uiText(u"双向")});
    }
    if (kind == "field" || kind == "contour") {
        choice("palette",uiText(u"色标"),{uiText(u"蓝-青-黄-红"),uiText(u"灰度"),uiText(u"蓝-白-红")});
        auto *range = new QLabel(uiText(u"真实结果读取后计算"),dialog); form->addRow(uiText(u"值域"),range);
    } else if (kind == "curve") {
        choice("xAxis",uiText(u"横轴"),{uiText(u"迭代步"),uiText(u"时间"),uiText(u"工况序号")});
    } else if (kind != "layer") {
        choice("field",uiText(u"显示字段"),{uiText(u"压力"),uiText(u"速度大小"),uiText(u"温度"),uiText(u"马赫数")});
    } else {
        auto *hint = new QLabel(uiText(u"通用对象尚未指定特征类型，请先创建切面、等值面或流线对象。"),dialog);
        hint->setWordWrap(true); form->addRow(hint);
    }
    layout->addLayout(form);
    auto *hint = new QLabel(uiText(u"仅保存当前对象的参数草稿，不执行数据读取、特征计算或专业参数校验。"),dialog);
    hint->setWordWrap(true); hint->setObjectName(QStringLiteral("mutedText")); layout->addWidget(hint); layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"保存草稿"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"取消")); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,dialog,[this, dialog, id, name, numbers, choices] {
        QVariantMap saved;
        for (auto it=numbers.cbegin();it!=numbers.cend();++it) saved.insert(it.key(),it.value()->value());
        for (auto it=choices.cbegin();it!=choices.cend();++it) saved.insert(it.key(),it.value()->currentIndex());
        parameterDrafts_[id]=saved;
        appendMessage(uiText(u"已保存“%1”的参数草稿；尚未执行计算。").arg(name));
        dialog->accept();
    });
    dialog->show();
}

void PostProcessWorkbenchWindow::showTaskPanel() {
    taskDock_->show(); taskDock_->raise();
}

void PostProcessWorkbenchWindow::showProbePanel() {
    if (!probeDock_) {
        probeDock_ = new QDockWidget(uiText(u"探针结果"), this);
        probeDock_->setObjectName(QStringLiteral("postProcessProbeDock"));
        installDockTitle(probeDock_);
        probeResult_ = new QTextEdit(probeDock_);
        probeResult_->setObjectName(QStringLiteral("postProcessProbeResult"));
        probeResult_->setReadOnly(true);
        probeResult_->setPlainText(uiText(u"暂无取点结果。

点击顶部“探针”启用取点工具，再在数据位置取点。当前 VTK 拾取服务尚未接入。"));
        probeResult_->setStyleSheet(centralWidget()->styleSheet());
        probeDock_->setWidget(probeResult_);
        addDockWidget(Qt::RightDockWidgetArea, probeDock_);
        resizeDocks({probeDock_}, {300}, Qt::Horizontal);
    }
    probeDock_->show(); probeDock_->raise();
}

void PostProcessWorkbenchWindow::displayProbeResult(const QString &text) {
    if (text.trimmed().isEmpty()) return;
    showProbePanel();
    probeResult_->setPlainText(text);
}

} // namespace aerosim
