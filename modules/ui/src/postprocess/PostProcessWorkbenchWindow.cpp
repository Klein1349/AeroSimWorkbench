#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
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
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QPixmap>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>

#include <functional>

namespace aerosim {
namespace {
enum InspectorPage {
    InspectorOverview = 0,
    InspectorZone,
    InspectorField,
    InspectorTime,
    InspectorCurve,
    InspectorDerived
};

template <size_t N> QString uiText(const char16_t (&text)[N])
{ return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1); }

QLabel *sectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent); label->setObjectName(uiText(u"sectionTitle")); return label;
}
QIcon commandIcon(const QString &kind) {
    QPixmap pixmap(32, 32); pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap); painter.setRenderHint(QPainter::Antialiasing);
    const QColor ink("#1d6fae"), accent("#16a6b6"), warm("#ebad37");
    QPen pen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin); painter.setPen(pen); painter.setBrush(Qt::NoBrush);
    if (kind == "import" || kind == "export") {
        const bool down = kind == "import"; painter.drawRoundedRect(5, 22, 22, 5, 1, 1);
        painter.drawLine(16, down ? 5 : 20, 16, down ? 18 : 7);
        QPolygon arrow; arrow << QPoint(11, down ? 15 : 10) << QPoint(16, down ? 20 : 5) << QPoint(21, down ? 15 : 10);
        painter.setBrush(ink); painter.drawPolygon(arrow);
    } else if (kind == "mesh") {
        painter.drawRoundedRect(4, 4, 24, 24, 2, 2); for (int p : {12, 20}) { painter.drawLine(p, 5, p, 27); painter.drawLine(5, p, 27, p); }
    } else if (kind == "contour") {
        painter.setPen(QPen(accent, 2.2)); painter.drawEllipse(4, 6, 24, 20); painter.setPen(QPen(ink, 2.2)); painter.drawEllipse(8, 9, 16, 14); painter.setPen(QPen(warm, 2.2)); painter.drawEllipse(12, 12, 8, 8);
    } else if (kind == "palette") {
        QLinearGradient gradient(5, 4, 27, 28); gradient.setColorAt(0, QColor("#1267a5")); gradient.setColorAt(.5, accent); gradient.setColorAt(1, warm); painter.setBrush(gradient); painter.setPen(QPen(ink, 1.5)); painter.drawRoundedRect(5, 5, 22, 22, 2, 2);
    } else if (kind == "slice") {
        QPolygon plane; plane << QPoint(6, 18) << QPoint(18, 8) << QPoint(28, 14) << QPoint(15, 25); painter.setBrush(QColor(22, 166, 182, 80)); painter.drawPolygon(plane); painter.setPen(QPen(ink, 1.5, Qt::DashLine)); painter.drawLine(4, 24, 28, 24);
    } else if (kind == "iso") {
        QPolygon top; top << QPoint(16, 4) << QPoint(27, 10) << QPoint(16, 16) << QPoint(5, 10); painter.setBrush(QColor(22, 166, 182, 80)); painter.drawPolygon(top); painter.setBrush(Qt::NoBrush); painter.drawLine(5, 10, 5, 22); painter.drawLine(27, 10, 27, 22); painter.drawLine(5, 22, 16, 28); painter.drawLine(27, 22, 16, 28); painter.drawLine(16, 16, 16, 28);
    } else if (kind == "stream" || kind == "trace") {
        painter.setPen(QPen(kind == "stream" ? accent : ink, 2.2, kind == "trace" ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap));
        for (int y : {8, 16, 24}) { QPainterPath path; path.moveTo(4, y); path.cubicTo(11, y - 7, 20, y + 7, 28, y); painter.drawPath(path); }
        painter.setBrush(kind == "trace" ? warm : accent); painter.setPen(Qt::NoPen); painter.drawEllipse(24, 13, 6, 6);
    } else if (kind == "select" || kind == "boxSelect") {
        painter.setPen(QPen(ink, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPolygon cursor; cursor << QPoint(7, 4) << QPoint(8, 25) << QPoint(13, 19) << QPoint(18, 28) << QPoint(22, 25) << QPoint(17, 16) << QPoint(27, 16);
        painter.setBrush(QColor(22, 166, 182, 90)); painter.drawPolygon(cursor);
        if (kind == "boxSelect") { painter.setBrush(Qt::NoBrush); painter.setPen(QPen(warm, 1.5, Qt::DashLine)); painter.drawRect(4, 5, 22, 19); }
    } else if (kind == "pan") {
        painter.setPen(QPen(ink, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(16, 4, 16, 28); painter.drawLine(4, 16, 28, 16);
        painter.setBrush(ink); for (const QPoint &p : {QPoint(16, 4), QPoint(16, 28), QPoint(4, 16), QPoint(28, 16)}) painter.drawEllipse(p, 2, 2);
    } else if (kind == "zoom") {
        painter.setPen(QPen(ink, 2.2)); painter.drawEllipse(5, 5, 15, 15); painter.drawLine(17, 17, 28, 28);
        painter.drawLine(9, 12, 16, 12); painter.drawLine(12, 9, 12, 16);
    } else if (kind == "rotate") {
        painter.setPen(QPen(accent, 2.2)); painter.drawArc(5, 5, 22, 22, 35 * 16, 270 * 16);
        painter.setBrush(accent); QPolygon arrow; arrow << QPoint(23, 5) << QPoint(29, 8) << QPoint(24, 12); painter.drawPolygon(arrow);
    } else if (kind == "top" || kind == "front" || kind == "right") {
        painter.setPen(QPen(ink, 2.0)); painter.setBrush(QColor(22, 166, 182, 70)); painter.drawRoundedRect(5, 6, 22, 20, 2, 2);
        painter.setPen(QPen(ink, 1.2)); painter.drawLine(5, 12, 27, 12); painter.drawLine(12, 6, 12, 26);
        painter.setPen(QPen(warm, 2.0)); painter.drawText(QRect(5, 6, 22, 20), Qt::AlignCenter, kind == "top" ? "T" : (kind == "front" ? "F" : "R"));
    } else if (kind == "fit") {
        for (const QPoint &point : {QPoint(5, 12), QPoint(27, 12), QPoint(5, 20), QPoint(27, 20)}) { const int sx = point.x() < 16 ? 1 : -1, sy = point.y() < 16 ? 1 : -1; painter.drawLine(point, point + QPoint(5 * sx, 0)); painter.drawLine(point, point + QPoint(0, 5 * sy)); }
    } else if (kind == "probe") {
        painter.drawEllipse(9, 9, 14, 14); painter.drawLine(16, 3, 16, 29); painter.drawLine(3, 16, 29, 16); painter.setBrush(warm); painter.drawEllipse(14, 14, 4, 4);
    } else if (kind == "analysis") {
        painter.drawLine(5, 27, 5, 5); painter.drawLine(5, 27, 28, 27); QPainterPath path; path.moveTo(7, 22); path.lineTo(13, 17); path.lineTo(18, 20); path.lineTo(26, 8); painter.setPen(QPen(accent, 2.5)); painter.drawPath(path);
    } else if (kind == "harness") {
        painter.setBrush(QColor(22, 166, 182, 70)); painter.setPen(pen); painter.drawEllipse(7, 7, 18, 18); painter.drawLine(16, 3, 16, 29); painter.drawLine(3, 16, 29, 16); painter.setBrush(warm); painter.setPen(Qt::NoPen); painter.drawEllipse(13, 13, 6, 6);
    } else {
        painter.drawRoundedRect(6, 5, 20, 22, 2, 2); painter.drawLine(10, 12, 22, 12); painter.drawLine(10, 18, 22, 18);
    }
    return QIcon(pixmap);
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
    explicit ResultPreviewWidget(QWidget *parent = nullptr) : QWidget(parent) { setMinimumSize(420, 360); }
protected:
    void paintEvent(QPaintEvent *) override {
        // Keep the preview illustrative, but use the same light workspace
        // surface as the main window's viewport placeholder.
        QPainter p(this); p.fillRect(rect(), QColor("#edf2f8"));
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
        const QRect legend(rect().right() - 45, 80, 13, qMax(120, height() - 175)); QLinearGradient lg(legend.topLeft(), legend.bottomLeft());
        lg.setColorAt(0, QColor("#d94e4e")); lg.setColorAt(.35, QColor("#f6c84b")); lg.setColorAt(.67, QColor("#10a4bd")); lg.setColorAt(1, QColor("#174f88")); p.fillRect(legend, lg); p.setPen(QColor("#8fa5bb")); p.drawRect(legend);
        p.setPen(QColor("#25364c")); p.drawText(QRect(20, 14, width() - 50, 30), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"三维结果预览"));
        p.setPen(QColor("#52677f")); p.drawText(QRect(20, height() - 34, width() - 40, 20), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"翼身组合 / 压力场 / 时间步 0"));
        p.drawText(QRect(0, 0, width() - 19, height()), Qt::AlignRight | Qt::AlignVCenter, uiText(u"压力"));
    }
};
}

PostProcessWorkbenchWindow::PostProcessWorkbenchWindow(QWidget *parent)
    : SARibbonMainWindow(parent) {
    setObjectName(uiText(u"postProcessWorkbench")); setWindowTitle(uiText(u"AeroSimWorkbench - 后处理工作台")); resize(1320, 820); setMinimumSize(1120, 680);
    QFont workbenchFont(uiText(u"Microsoft YaHei UI")); workbenchFont.setPointSize(9); setFont(workbenchFont);
    createActions();
    auto *workbench = new QWidget(this); auto *root = new QVBoxLayout(workbench); root->setContentsMargins(7, 7, 7, 0); root->setSpacing(6);
    auto *content = new QSplitter(Qt::Horizontal, workbench); content->setObjectName(uiText(u"postProcessContent"));
    auto *tabs = new QTabWidget(content); tabs->setObjectName(uiText(u"postProcessNavigator")); tabs->setMinimumWidth(280); tabs->setMaximumWidth(360);
    tabs->addTab(createViewPanel(), uiText(u"视图")); tabs->addTab(createDataPanel(), uiText(u"数据")); tabs->addTab(createObjectPanel(), uiText(u"对象"));
    content->addWidget(tabs); content->addWidget(createViewportPanel()); content->addWidget(createPropertiesPanel()); content->setStretchFactor(1, 1); content->setSizes({300, 830, 320}); content->setChildrenCollapsible(false);
    root->addWidget(content, 1); root->addWidget(createTimeline()); setCentralWidget(workbench);
    auto *dock = new QDockWidget(uiText(u"操作消息"), this); dock->setObjectName(uiText(u"postProcessMessageDock")); events_ = new QTextEdit(dock); events_->setReadOnly(true); dock->setWidget(events_); addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide();
    statusBar()->showMessage(uiText(u"已加载翼身组合结果集：从左侧选择数据，再在 Ribbon 中创建显示或分析对象。"));
    // The post-processing layout is intentionally independent from MainWindow.
    // It shares MainWindow's restrained workspace system: one visual boundary
    // per region, calm surfaces, and blue only for selection or primary work.
    setStyleSheet(uiText(u"QMainWindow{background:#edf2f8;color:#25364c;}QSplitter::handle{background:#dce2eb;}QSplitter::handle:horizontal{width:1px;}QTabWidget::pane{background:#fff;border:0;}QTabBar::tab{padding:8px 15px;background:transparent;color:#52677f;border:0;border-bottom:2px solid transparent;}QTabBar::tab:selected{background:#fff;color:#234d7a;border-bottom-color:#4687b6;}QFrame#viewportFrame{background:#fff;border:1px solid #dce2eb;}QFrame#viewportHeader{background:#f4f7fb;border:0;border-bottom:1px solid #dce2eb;}QLabel#viewportTitle,QLabel#sectionTitle{color:#234d7a;font-weight:600;}QLabel#viewportOrientation{color:#234d7a;font-weight:600;border-left:1px solid #dce2eb;padding-left:10px;}QTreeWidget,QListWidget{border:0;background:#fff;outline:0;alternate-background-color:#f4f7fb;}QTreeWidget::item,QListWidget::item{min-height:27px;padding:4px;}QTreeWidget::item:selected,QListWidget::item:selected{background:#dceafd;color:#153b6b;}QFrame#dataCard{background:#f4f7fb;border:0;border-radius:4px;}QFrame#postProcessTimeline{background:#fff;border:0;border-top:1px solid #dce2eb;border-radius:0;}QComboBox,QLineEdit{min-height:27px;border:1px solid #dce2eb;border-radius:3px;padding:2px 7px;background:#fff;}QPushButton{min-height:28px;border:1px solid #dce2eb;border-radius:3px;background:#fff;padding:6px 12px;}QPushButton:hover{background:#dceafd;border-color:#a8c6e5;}QPushButton#primaryAction{background:#234d7a;color:#fff;border:0;font-weight:600;}QLabel#mutedText{color:#52677f;font-size:12px;}QGroupBox{margin-top:10px;padding:6px 0 0 0;border:0;font-weight:600;}QGroupBox::title{subcontrol-origin:margin;left:0;padding:0;color:#234d7a;}QStatusBar{background:#e9eef5;color:#25364c;}QTextEdit{background:#fff;color:#25364c;border:0;}"));
    updateDataSelection(datasetTree_->currentItem(), 0);
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
    for (QAction *action : {selectAction, boxSelectAction, panAction, zoomAction, rotateAction}) {
        action->setCheckable(true);
        interactionModes->addAction(action);
    }
    selectAction->setChecked(true);
    const QList<QString> analysisNames = {uiText(u"收敛曲线"), uiText(u"六分量汇总"), uiText(u"公式与舵效"), uiText(u"系列线与图表"), uiText(u"AI 气动预测")};
    QList<QAction *> analysisActions;
    for (const QString &name : analysisNames) {
        analysisActions.append(ribbonAction(this, commandIcon("analysis"), name,
                                            uiText(u"打开") + name + uiText(u"工作流程"),
                                            uiText(u"postProcessAnalysisAction")));
    }

    // Use the same SARibbon hierarchy as MainWindow.  Categories group work
    // by intent; the data/scene workspace below remains the post-process UI.
    auto *dataCategory = ribbonBar()->addCategoryPage(uiText(u"数据"));
    dataCategory->setObjectName(uiText(u"postProcessDataCategory"));
    auto *dataSourcePanel = dataCategory->addPanel(uiText(u"结果数据"));
    dataSourcePanel->addLargeAction(importAction); dataSourcePanel->addLargeAction(exportAction);
    auto *dataToolsPanel = dataCategory->addPanel(uiText(u"数据处理"));
    dataToolsPanel->addLargeAction(convertAction); dataToolsPanel->addLargeAction(excelAction);

    auto *displayCategory = ribbonBar()->addCategoryPage(uiText(u"显示"));
    displayCategory->setObjectName(uiText(u"postProcessDisplayCategory"));
    auto *interactionPanel = displayCategory->addPanel(uiText(u"交互"));
    interactionPanel->addLargeAction(selectAction); interactionPanel->addLargeAction(boxSelectAction);
    interactionPanel->addLargeAction(panAction); interactionPanel->addLargeAction(zoomAction); interactionPanel->addLargeAction(rotateAction);
    auto *layerPanel = displayCategory->addPanel(uiText(u"图层"));
    layerPanel->addLargeAction(meshAction); layerPanel->addLargeAction(contourAction); layerPanel->addLargeAction(colorAction);
    auto *cameraPanel = displayCategory->addPanel(uiText(u"视图"));
    cameraPanel->addLargeAction(fitAction); cameraPanel->addLargeAction(frontAction); cameraPanel->addLargeAction(rightAction); cameraPanel->addLargeAction(topAction);

    auto *featureCategory = ribbonBar()->addCategoryPage(uiText(u"特征"));
    featureCategory->setObjectName(uiText(u"postProcessFeatureCategory"));
    auto *spatialPanel = featureCategory->addPanel(uiText(u"空间特征"));
    spatialPanel->addLargeAction(sliceAction); spatialPanel->addLargeAction(isoAction);
    spatialPanel->addLargeAction(streamAction); spatialPanel->addLargeAction(traceAction);

    auto *analysisCategory = ribbonBar()->addCategoryPage(uiText(u"分析"));
    analysisCategory->setObjectName(uiText(u"postProcessAnalysisCategory"));
    auto *analysisPanel = analysisCategory->addPanel(uiText(u"结果分析"));
    for (QAction *action : analysisActions) analysisPanel->addLargeAction(action);

    auto *toolsCategory = ribbonBar()->addCategoryPage(uiText(u"工具"));
    toolsCategory->setObjectName(uiText(u"postProcessToolsCategory"));
    auto *queryPanel = toolsCategory->addPanel(uiText(u"查询与导出"));
    queryPanel->addLargeAction(probeAction); queryPanel->addLargeAction(geometryExportAction); queryPanel->addLargeAction(animationExportAction);
    auto *assistantPanel = toolsCategory->addPanel(uiText(u"智能辅助"));
    assistantPanel->addLargeAction(deepSeekAction);

    auto *interactionHint = new QLabel(uiText(u"选择模式 · 左键选择 / 中键平移 / 滚轮缩放"), this);
    interactionHint->setObjectName(uiText(u"postProcessInteractionHint"));
    statusBar()->addPermanentWidget(interactionHint, 1);
    featureActions_ = {meshAction, contourAction, sliceAction, isoAction, streamAction, traceAction};
    const auto command = [this](const QString &key, const QString &message) { emit postProcessCommandRequested(key); appendMessage(message); };
    connect(importAction, &QAction::triggered, this, [command] { command(uiText(u"importResults"), uiText(u"已打开导入结果流程；结果读取服务接入后将填充数据集树。")); });
    connect(exportAction, &QAction::triggered, this, [command] { command(uiText(u"export"), uiText(u"已提交导出请求；请在图层页选择导出对象。")); });
    const auto setInteractionMode = [this, interactionHint](const QString &mode, const QString &hint) {
        interactionHint->setText(hint);
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
    connect(colorAction, &QAction::triggered, this, [command] { command(uiText(u"configureColorMap"), uiText(u"请在右侧字段与色标页面配置色标和值域。")); });
    const auto contourFn = [this] { addDerivedLayer(uiText(u"contour"), uiText(u"压力云图"), uiText(u"已将“压力”以云图添加到图层。")); }; connect(contourAction, &QAction::triggered, this, contourFn);
    const auto sliceFn = [this] { addDerivedLayer(uiText(u"slice"), uiText(u"切面 1（X = 0.0）"), uiText(u"已创建切面；可在右侧修改位置与显示字段。")); }; connect(sliceAction, &QAction::triggered, this, sliceFn);
    const auto isoFn = [this] { addDerivedLayer(uiText(u"iso"), uiText(u"等值面 1（压力）"), uiText(u"已创建等值面；请设置等值以便后续服务执行。")); }; connect(isoAction, &QAction::triggered, this, isoFn);
    const auto streamFn = [this] { addDerivedLayer(uiText(u"stream"), uiText(u"流线 1"), uiText(u"已创建流线对象；请在右侧设置种子和积分方向。")); }; connect(streamAction, &QAction::triggered, this, streamFn);
    const auto traceFn = [this] { addDerivedLayer(uiText(u"trace"), uiText(u"粒子轨迹 1"), uiText(u"已创建轨迹对象；时序数据接入后可运行追踪。")); }; connect(traceAction, &QAction::triggered, this, traceFn);
    connect(fitAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"reset")); appendMessage(uiText(u"视图已适配所有可见图层。")); }); connect(probeAction, &QAction::triggered, this, [command] { command(uiText(u"probe"), uiText(u"已进入探针查询模式：请在视口点选单元或节点。")); });
    connect(deepSeekAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::openDeepSeekHarness);
    connect(probeAction, &QAction::triggered, this, &PostProcessWorkbenchWindow::showProbePanel);
    for (QAction *action : analysisActions) connect(action, &QAction::triggered, this, [command, action] { command(uiText(u"openAnalysis"), uiText(u"已打开“%1”工作流程入口。").arg(action->text())); });
}

void PostProcessWorkbenchWindow::openDeepSeekHarness() {
    if (!deepSeekHarnessDock_) {
        deepSeekHarnessDock_ = new QDockWidget(uiText(u"DeepSeek Harness"), this);
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
        deepSeekHarnessDock_->setWidget(panel);
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
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8); auto *row = new QHBoxLayout; row->addWidget(sectionLabel(uiText(u"当前结果集"), panel)); row->addStretch(); auto *import = new QPushButton(uiText(u"＋ 导入"), panel); import->setObjectName(uiText(u"primaryAction")); row->addWidget(import); layout->addLayout(row);
    auto *card = new QFrame(panel); card->setObjectName(uiText(u"dataCard")); auto *cardLayout = new QVBoxLayout(card); sessionContext_ = new QLabel(uiText(u"翼身组合气动算例 / 结果集"), card); sessionContext_->setStyleSheet(uiText(u"font-weight:600;color:#25364c;")); auto *description = new QLabel(uiText(u"选择 Zone、边界或字段后，工具栏仅点亮当前数据支持的功能。"), card); description->setObjectName(uiText(u"mutedText")); description->setWordWrap(true); cardLayout->addWidget(sessionContext_); cardLayout->addWidget(description); layout->addWidget(card);
    selectionSummary_ = new QLabel(uiText(u"当前选择：流场体网格"), panel); selectionSummary_->setObjectName(uiText(u"mutedText")); layout->addWidget(selectionSummary_); datasetTree_ = new QTreeWidget(panel); datasetTree_->setObjectName(uiText(u"postProcessDatasetTree")); datasetTree_->setHeaderHidden(true); datasetTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *zones = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"■  网格与 Zone"))); zones->setData(0, Qt::UserRole, "group"); auto *volume = new QTreeWidgetItem(zones, QStringList(uiText(u"◆  流场体网格"))); volume->setData(0, Qt::UserRole, "volume"); for (const QString &text : {uiText(u"翼面 Zone"), uiText(u"机身壁面")}) { auto *item = new QTreeWidgetItem(zones, QStringList(uiText(u"◆  ") + text)); item->setData(0, Qt::UserRole, "surface"); }
    auto *fields = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"◉  物理字段"))); fields->setData(0, Qt::UserRole, "group"); for (const QString &text : {uiText(u"压力 [Pa]"), uiText(u"速度大小 [m/s]"), uiText(u"温度 [K]"), uiText(u"马赫数 [-]")}) { auto *item = new QTreeWidgetItem(fields, QStringList(uiText(u"●  ") + text)); item->setData(0, Qt::UserRole, "field"); }
    auto *time = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"◷  时间序列"))); time->setData(0, Qt::UserRole, "time"); for (const QString &text : {uiText(u"t = 0.0 s"), uiText(u"t = 0.5 s"), uiText(u"t = 1.0 s")}) { auto *item = new QTreeWidgetItem(time, QStringList(text)); item->setData(0, Qt::UserRole, "time"); }
    auto *monitoring = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"∿  监控数据"))); monitoring->setData(0, Qt::UserRole, "group"); auto *curve = new QTreeWidgetItem(monitoring, QStringList(uiText(u"∿  阻力系数监控曲线"))); curve->setData(0, Qt::UserRole, "curve"); datasetTree_->expandAll(); datasetTree_->setCurrentItem(volume); layout->addWidget(datasetTree_, 1); auto *hint = new QLabel(uiText(u"数据集中的 Zone、字段与时间步由读取服务提供；派生对象在“对象”页管理。"), panel); hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint); connect(datasetTree_, &QTreeWidget::itemClicked, this, &PostProcessWorkbenchWindow::updateDataSelection); connect(datasetTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) { updateDataSelection(current, 0); }); connect(import, &QPushButton::clicked, this, [this] { emit postProcessCommandRequested(uiText(u"importResults")); appendMessage(uiText(u"已打开结果导入流程。")); }); return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(sectionLabel(uiText(u"Frame 显示控制"), panel));
    auto *hint = new QLabel(uiText(u"这些开关作用于当前 Frame 的所有 Zone；单个 Zone 的样式请在右侧“属性”页设置。"), panel);
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

    auto *scope = new QFrame(panel); scope->setObjectName(uiText(u"dataCard")); auto *scopeLayout = new QVBoxLayout(scope);
    scopeLayout->addWidget(sectionLabel(uiText(u"作用域"), scope));
    auto *scopeHint = new QLabel(uiText(u"当前 Frame · 三维笛卡尔 · 所有 Zone"), scope); scopeHint->setObjectName(uiText(u"mutedText")); scopeLayout->addWidget(scopeHint);
    layout->addWidget(scope); layout->addStretch();
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createObjectPanel() {
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(sectionLabel(uiText(u"对象与图层"), panel));
    auto *hint = new QLabel(uiText(u"对象属于当前 Frame。选择对象后，在右侧编辑其参数；显隐和删除只影响选中对象。"), panel);
    hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint);
    auto *row = new QHBoxLayout;
    row->addWidget(sectionLabel(uiText(u"派生对象"), panel)); row->addStretch();
    auto *add = new QPushButton(uiText(u"＋ 新建"), panel); row->addWidget(add); layout->addLayout(row);
    sceneList_ = new QListWidget(panel); sceneList_->setObjectName(uiText(u"sceneLayerList"));
    sceneList_->addItems({uiText(u"◈  切面 1（X = 0.0）  可见"), uiText(u"◇  等值面 1（压力） 可见"), uiText(u"≈  流线 1           可见")});
    sceneList_->setCurrentRow(0); layout->addWidget(sceneList_, 1);
    auto *actions = new QHBoxLayout; auto *visible = new QPushButton(uiText(u"显隐"), panel); auto *remove = new QPushButton(uiText(u"删除"), panel); actions->addWidget(visible); actions->addWidget(remove); layout->addLayout(actions);
    connect(add, &QPushButton::clicked, this, [this] { addDerivedLayer(uiText(u"layer"), uiText(u"新派生对象"), uiText(u"已创建派生对象，请在右侧配置。")); });
    connect(sceneList_, &QListWidget::itemSelectionChanged, this, &PostProcessWorkbenchWindow::updateSelectionDetails);
    connect(visible, &QPushButton::clicked, this, [this] {
        auto *item = sceneList_->currentItem(); if (!item) return;
        const bool isVisible = item->text().endsWith(uiText(u"可见"));
        item->setText(item->text().left(item->text().lastIndexOf(uiText(u"  "))) + (isVisible ? uiText(u"  隐藏") : uiText(u"  可见")));
        appendMessage(isVisible ? uiText(u"已隐藏选中派生对象。") : uiText(u"已显示选中派生对象。"));
    });
    connect(remove, &QPushButton::clicked, this, [this] { if (sceneList_->currentItem()) { delete sceneList_->takeItem(sceneList_->currentRow()); appendMessage(uiText(u"已移除选中派生对象。")); } });
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewportPanel() {
    auto *frame = new QFrame(this); frame->setObjectName(uiText(u"viewportFrame"));
    auto *layout = new QVBoxLayout(frame); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *header = new QFrame(frame); header->setObjectName(uiText(u"viewportHeader")); auto *headerLayout = new QHBoxLayout(header); headerLayout->setContentsMargins(10, 4, 10, 4);
    viewportTitle_ = new QLabel(uiText(u"Frame 1 · 三维笛卡尔"), header); viewportTitle_->setObjectName(uiText(u"viewportTitle"));
    viewportHint_ = new QLabel(uiText(u"翼身组合 · 压力 [Pa] · t = 0.0 s"), header); viewportHint_->setObjectName(uiText(u"mutedText"));
    auto *orientation = new QLabel(uiText(u"Z ↑   Y →"), header); orientation->setObjectName(uiText(u"viewportOrientation"));
    headerLayout->addWidget(viewportTitle_); headerLayout->addStretch(); headerLayout->addWidget(viewportHint_); headerLayout->addSpacing(16); headerLayout->addWidget(orientation);
    layout->addWidget(header);
    auto *viewportHost = new QFrame(frame); viewportHost->setObjectName(uiText(u"postProcessViewportHost")); viewportLayout_ = new QVBoxLayout(viewportHost); viewportLayout_->setContentsMargins(0, 0, 0, 0);
    emptyViewport_ = new ResultPreviewWidget(viewportHost); emptyViewport_->setObjectName(uiText(u"postProcessResultPreview")); viewportLayout_->addWidget(emptyViewport_);
    layout->addWidget(viewportHost, 1); return frame;
}

// Kept as a historical prototype while the context-specific inspector below is active.
#if 0
QWidget *PostProcessWorkbenchWindow::createPropertiesPanel() {
    auto *panel = new QWidget(this); panel->setMinimumWidth(290); panel->setMaximumWidth(370); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8); layout->addWidget(sectionLabel(uiText(u"选中对象"), panel)); propertiesStack_ = new QStackedWidget(panel); propertiesStack_->setObjectName(uiText(u"postProcessProperties")); auto *none = new QLabel(uiText(u"在数据集或图层中选择对象，以查看其属性与可用操作。"), propertiesStack_); none->setAlignment(Qt::AlignCenter); none->setWordWrap(true); propertiesStack_->addWidget(none); auto *details = new QWidget(propertiesStack_); auto *detailsLayout = new QVBoxLayout(details); propertyContextTitle_ = new QLabel(details); propertyContextTitle_->setStyleSheet(uiText(u"font-size:15px;font-weight:600;color:#234d7a;")); propertyContextHint_ = new QLabel(details); propertyContextHint_->setObjectName(uiText(u"mutedText")); propertyContextHint_->setWordWrap(true); detailsLayout->addWidget(propertyContextTitle_); detailsLayout->addWidget(propertyContextHint_); auto *identity = new QGroupBox(uiText(u"对象"), details); auto *identityForm = new QFormLayout(identity); selectionName_ = new QLineEdit(identity); selectionName_->setReadOnly(true); identityForm->addRow(uiText(u"名称"), selectionName_); auto *show = new QCheckBox(uiText(u"在视口中显示"), identity); show->setChecked(true); identityForm->addRow(show); detailsLayout->addWidget(identity); auto *display = new QGroupBox(uiText(u"显示设置"), details); auto *displayForm = new QFormLayout(display); auto *field = new QComboBox(display); field->addItems({uiText(u"压力"), uiText(u"速度大小"), uiText(u"温度"), uiText(u"马赫数")}); auto *style = new QComboBox(display); style->addItems({uiText(u"云图"), uiText(u"网格线"), uiText(u"实体表面"), uiText(u"边线")}); auto *opacity = new QSlider(Qt::Horizontal, display); opacity->setRange(0,100); opacity->setValue(100); displayForm->addRow(uiText(u"字段"),field); displayForm->addRow(uiText(u"显示方式"),style); displayForm->addRow(uiText(u"不透明度"),opacity); detailsLayout->addWidget(display); auto *parameters = new QGroupBox(uiText(u"特征与分析"), details); auto *form = new QFormLayout(parameters); auto *parameter = new QLineEdit(uiText(u"根据当前对象配置"), parameters); parameter->setReadOnly(true); form->addRow(uiText(u"参数"), parameter); detailsLayout->addWidget(parameters); detailsLayout->addStretch(); propertiesStack_->addWidget(details); layout->addWidget(propertiesStack_,1); return panel;
}

#endif

QWidget *PostProcessWorkbenchWindow::createPropertiesPanel()
{
    auto *panel = new QWidget(this);
    panel->setMinimumWidth(290);
    panel->setMaximumWidth(370);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    inspectorTabs_ = new QTabWidget(panel); inspectorTabs_->setObjectName(uiText(u"postProcessInspectorTabs"));
    auto *propertiesPage = new QWidget(inspectorTabs_); auto *propertiesLayout = new QVBoxLayout(propertiesPage); propertiesLayout->setContentsMargins(4, 6, 4, 4);
    propertiesStack_ = new QStackedWidget(propertiesPage);
    propertiesStack_->setObjectName(uiText(u"postProcessProperties"));

    auto *overview = new QLabel(uiText(u"选择 Zone、字段、时间步、曲线或派生对象。\n\n右侧只展示与当前选择匹配的设置；视图页的全局图层开关不会改变这里。"), propertiesStack_);
    overview->setWordWrap(true); overview->setAlignment(Qt::AlignCenter);
    propertiesStack_->addWidget(overview);

    auto *zone = new QWidget(propertiesStack_); auto *zoneLayout = new QVBoxLayout(zone);
    zoneLayout->addWidget(sectionLabel(uiText(u"Zone 信息与局部样式"), zone));
    zoneSummary_ = new QLabel(zone); zoneSummary_->setWordWrap(true); zoneLayout->addWidget(zoneSummary_);
    auto *zoneStyle = new QGroupBox(uiText(u"当前 Zone"), zone); auto *zoneForm = new QFormLayout(zoneStyle);
    auto *zoneVisible = new QCheckBox(uiText(u"在当前 Frame 显示"), zoneStyle); zoneVisible->setChecked(true);
    auto *zoneAppearance = new QComboBox(zoneStyle); zoneAppearance->addItems({uiText(u"继承绘图层设置"), uiText(u"仅显示边线"), uiText(u"隐藏表面")});
    zoneForm->addRow(zoneVisible); zoneForm->addRow(uiText(u"局部样式"), zoneAppearance); zoneLayout->addWidget(zoneStyle); zoneLayout->addStretch(); propertiesStack_->addWidget(zone);

    auto *field = new QWidget(propertiesStack_); auto *fieldLayout = new QVBoxLayout(field);
    fieldLayout->addWidget(sectionLabel(uiText(u"字段与色标"), field)); fieldSummary_ = new QLabel(field); fieldSummary_->setWordWrap(true); fieldLayout->addWidget(fieldSummary_);
    auto *colorMap = new QGroupBox(uiText(u"云图映射"), field); auto *colorForm = new QFormLayout(colorMap);
    auto *palette = new QComboBox(colorMap); palette->addItems({uiText(u"蓝-青-黄-红"), uiText(u"灰度"), uiText(u"蓝-白-红")});
    auto *range = new QLineEdit(uiText(u"由当前结果自动计算"), colorMap); range->setReadOnly(true);
    colorForm->addRow(uiText(u"色标"), palette); colorForm->addRow(uiText(u"值域"), range); fieldLayout->addWidget(colorMap); fieldLayout->addStretch(); propertiesStack_->addWidget(field);

    auto *time = new QWidget(propertiesStack_); auto *timeLayout = new QVBoxLayout(time);
    timeLayout->addWidget(sectionLabel(uiText(u"时间控制"), time)); timeSummary_ = new QLabel(time); timeSummary_->setWordWrap(true); timeLayout->addWidget(timeSummary_);
    auto *timeHint = new QLabel(uiText(u"时间步是当前 Frame 的全局状态。请用底部时间轴播放、步进或定位；它不是图层，也没有透明度或显示字段。"), time);
    timeHint->setObjectName(uiText(u"mutedText")); timeHint->setWordWrap(true); timeLayout->addWidget(timeHint); timeLayout->addStretch(); propertiesStack_->addWidget(time);

    auto *curve = new QWidget(propertiesStack_); auto *curveLayout = new QVBoxLayout(curve);
    curveLayout->addWidget(sectionLabel(uiText(u"曲线与监控量"), curve)); curveSummary_ = new QLabel(curve); curveSummary_->setWordWrap(true); curveLayout->addWidget(curveSummary_);
    auto *curveStyle = new QGroupBox(uiText(u"曲线显示"), curve); auto *curveForm = new QFormLayout(curveStyle);
    auto *xAxis = new QComboBox(curveStyle); xAxis->addItems({uiText(u"迭代步"), uiText(u"时间"), uiText(u"工况序号")}); curveForm->addRow(uiText(u"横轴"), xAxis); curveLayout->addWidget(curveStyle); curveLayout->addStretch(); propertiesStack_->addWidget(curve);

    auto *derived = new QWidget(propertiesStack_); auto *derivedLayout = new QVBoxLayout(derived);
    derivedLayout->addWidget(sectionLabel(uiText(u"派生对象参数"), derived)); derivedSummary_ = new QLabel(derived); derivedSummary_->setWordWrap(true); derivedLayout->addWidget(derivedSummary_);
    auto *derivedParameter = new QGroupBox(uiText(u"对象参数"), derived); auto *derivedForm = new QFormLayout(derivedParameter);
    auto *source = new QLineEdit(uiText(u"由创建该对象时的 Zone 与字段确定"), derivedParameter); source->setReadOnly(true);
    auto *parameter = new QLineEdit(uiText(u"请按对象类型配置"), derivedParameter); parameter->setReadOnly(true);
    derivedForm->addRow(uiText(u"数据来源"), source); derivedForm->addRow(uiText(u"关键参数"), parameter); derivedLayout->addWidget(derivedParameter); derivedLayout->addStretch(); propertiesStack_->addWidget(derived);

    auto *probe = new QWidget(inspectorTabs_); auto *probeLayout = new QVBoxLayout(probe);
    probeLayout->addWidget(sectionLabel(uiText(u"Probe 查询"), probe)); probeSummary_ = new QLabel(probe); probeSummary_->setWordWrap(true); probeLayout->addWidget(probeSummary_);
    auto *probeResult = new QGroupBox(uiText(u"查询结果"), probe); auto *probeForm = new QFormLayout(probeResult);
    auto *cell = new QLineEdit(uiText(u"等待视口点选"), probeResult); cell->setReadOnly(true);
    auto *values = new QLineEdit(uiText(u"字段值将在此显示"), probeResult); values->setReadOnly(true);
    probeForm->addRow(uiText(u"单元 / 节点"), cell); probeForm->addRow(uiText(u"字段值"), values); probeLayout->addWidget(probeResult); probeLayout->addStretch();

    propertiesLayout->addWidget(propertiesStack_);
    inspectorTabs_->addTab(propertiesPage, uiText(u"属性"));
    inspectorTabs_->addTab(probe, uiText(u"Probe"));
    auto *tasks = new QWidget(inspectorTabs_); auto *taskLayout = new QVBoxLayout(tasks);
    taskLayout->addWidget(sectionLabel(uiText(u"任务与消息"), tasks));
    auto *taskHint = new QLabel(uiText(u"读取、特征计算与导出任务会在这里显示进度、版本与错误。当前没有活动任务。"), tasks);
    taskHint->setObjectName(uiText(u"mutedText")); taskHint->setWordWrap(true); taskLayout->addWidget(taskHint); taskLayout->addStretch();
    inspectorTabs_->addTab(tasks, uiText(u"任务"));
    layout->addWidget(inspectorTabs_, 1);
    return panel;
}

QWidget *PostProcessWorkbenchWindow::createTimeline() {
    auto *frame = new QFrame(this); frame->setObjectName(uiText(u"postProcessTimeline"));
    auto *layout = new QHBoxLayout(frame); layout->setContentsMargins(8,5,8,5);
    layout->addWidget(new QLabel(uiText(u"时间"), frame));
    const auto addStepButton = [this, frame, layout](const QString &text, const QString &name, const std::function<int(int)> &next) {
        auto *button = new QPushButton(text, frame); button->setObjectName(name); button->setFixedWidth(34); button->setToolTip(text); layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, next] { timeSlider_->setValue(next(timeSlider_->value())); });
    };
    addStepButton(uiText(u"|◀"), uiText(u"postProcessFirstFrame"), [](int) { return 0; });
    addStepButton(uiText(u"◀"), uiText(u"postProcessPreviousFrame"), [](int value) { return qMax(0, value - 1); });
    addStepButton(uiText(u"▶"), uiText(u"postProcessNextFrame"), [](int value) { return qMin(2, value + 1); });
    addStepButton(uiText(u"▶|"), uiText(u"postProcessLastFrame"), [](int) { return 2; });
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
    const QString type = item->data(0, Qt::UserRole).toString();
    const QString name = item->text(0).mid(3).trimmed();
    selectionSummary_->setText(uiText(u"当前选择：") + name);
    if (type == "time") {
        const int timeIndex = name.contains(uiText(u"1.0")) ? 2 : name.contains(uiText(u"0.5")) ? 1 : 0;
        timeSlider_->setValue(timeIndex);
        timeSummary_->setText(uiText(u"当前 Frame 已切换至 %1。\n\n切换此状态会更新视口中的所有 Zone 与派生对象。").arg(name));
        propertiesStack_->setCurrentIndex(InspectorTime);
        inspectorTabs_->setCurrentIndex(0);
        setFeatureActionsEnabled(true, true, true, true, true);
        return;
    }
    const bool spatial = type == "volume" || type == "surface" || type == "field";
    const bool field = type == "field";
    if (type == "volume" || type == "surface") {
        zoneSummary_->setText(uiText(u"已选择 %1。\n\n这里仅配置该 Zone 的局部可见性与样式；左侧“视图”页控制整个 Frame 的全局图层。").arg(name));
        propertiesStack_->setCurrentIndex(InspectorZone);
    } else if (field) {
        fieldSummary_->setText(uiText(u"已选择字段：%1。\n\n可调整色标和值域，并将其用于云图、切面或等值面。").arg(name));
        propertiesStack_->setCurrentIndex(InspectorField);
    } else if (type == "curve") {
        curveSummary_->setText(uiText(u"已选择监控曲线：%1。\n\n此处控制曲线坐标轴和显示样式，不影响三维视口图层。").arg(name));
        propertiesStack_->setCurrentIndex(InspectorCurve);
    } else {
        propertiesStack_->setCurrentIndex(InspectorOverview);
    }
    inspectorTabs_->setCurrentIndex(0);
    setFeatureActionsEnabled(spatial, spatial, type == "volume" || field,
                             type == "volume" || field,
                             type == "volume" || type == "surface" || field);
}
void PostProcessWorkbenchWindow::addDerivedLayer(const QString &kind,const QString &name,const QString &message){emit postProcessCommandRequested(kind);if(sceneList_){sceneList_->addItem(uiText(u"◉  ")+name+uiText(u"  可见"));sceneList_->setCurrentRow(sceneList_->count()-1);}appendMessage(message);}
bool PostProcessWorkbenchWindow::insertWidget(WidgetSlot slot,QWidget *widget){if(slot!=WidgetSlot::Viewport||!widget||!viewportLayout_)return false;if(emptyViewport_){viewportLayout_->removeWidget(emptyViewport_);emptyViewport_->deleteLater();emptyViewport_=nullptr;}if(viewportLayout_->count()!=0)return false;viewportLayout_->addWidget(widget);return true;}
void PostProcessWorkbenchWindow::bindServices(const ServiceBindings &services){if(bound_)return;services_=services;bound_=true;if(services_.tasks)connect(services_.tasks,&ITaskService::taskEventOccurred,this,[this](const TaskEvent &event){appendMessage(event.message);},Qt::QueuedConnection);}
void PostProcessWorkbenchWindow::appendMessage(const QString &message){events_->append(message.toHtmlEscaped());statusBar()->showMessage(message,5000);}
void PostProcessWorkbenchWindow::updateSelectionDetails()
{
    const auto selected = sceneList_->selectedItems();
    if (selected.isEmpty()) { propertiesStack_->setCurrentIndex(InspectorOverview); inspectorTabs_->setCurrentIndex(0); return; }
    const QString name = selected.first()->text().mid(3).trimmed();
    const QString kind = name.contains(uiText(u"切面")) ? uiText(u"切面：位置、法向和显示字段")
        : name.contains(uiText(u"等值面")) ? uiText(u"等值面：字段、等值与表面样式")
        : name.contains(uiText(u"流线")) ? uiText(u"流线：种子、积分方向和终止条件")
        : name.contains(uiText(u"轨迹")) ? uiText(u"轨迹：粒子种子与时间范围")
        : uiText(u"派生对象：按对象类型配置参数");
    derivedSummary_->setText(uiText(u"已选择：%1\n\n%2").arg(name, kind));
    propertiesStack_->setCurrentIndex(InspectorDerived);
    inspectorTabs_->setCurrentIndex(0);
}

void PostProcessWorkbenchWindow::showProbePanel()
{
    probeSummary_->setText(uiText(u"Probe 模式已开启。\n\n请在中央视口点选节点、单元或派生对象；查询结果将显示在下方。"));
    inspectorTabs_->setCurrentIndex(1);
}
} // namespace aerosim
