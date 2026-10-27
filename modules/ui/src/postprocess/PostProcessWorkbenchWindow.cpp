#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{ return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1); }

QLabel *sectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent); label->setObjectName(uiText(u"sectionTitle")); return label;
}
QToolButton *toolButton(QToolBar *bar, const QString &text, const QString &tip) {
    auto *button = new QToolButton(bar); button->setText(text); button->setToolTip(tip);
    button->setProperty("availableToolTip", tip); button->setToolButtonStyle(Qt::ToolButtonTextOnly); bar->addWidget(button); return button;
}

// Shown only until a real VTK widget is inserted. This is a visual preview, not a fake data source.
class ResultPreviewWidget final : public QWidget {
public:
    explicit ResultPreviewWidget(QWidget *parent = nullptr) : QWidget(parent) { setMinimumSize(420, 360); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.fillRect(rect(), QColor("#162536"));
        const QRect area = rect().adjusted(28, 58, -86, -48);
        QLinearGradient gradient(area.topLeft(), area.bottomRight());
        gradient.setColorAt(0, QColor("#174f88")); gradient.setColorAt(.32, QColor("#10a4bd")); gradient.setColorAt(.58, QColor("#79c76f")); gradient.setColorAt(.78, QColor("#f6c84b")); gradient.setColorAt(1, QColor("#d94e4e"));
        QPainterPath body; body.moveTo(area.left() + 12, area.center().y());
        body.cubicTo(area.left() + area.width() * .27, area.top() + 14, area.left() + area.width() * .68, area.top() + 24, area.right() - 12, area.center().y() - 8);
        body.cubicTo(area.left() + area.width() * .68, area.bottom() - 20, area.left() + area.width() * .25, area.bottom() - 24, area.left() + 12, area.center().y());
        p.setBrush(gradient); p.setPen(Qt::NoPen); p.drawPath(body);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(255,255,255,90), 1));
        for (int i = 1; i < 9; ++i) { const int x = area.left() + area.width() * i / 9; p.drawLine(x, area.top() + 30, x, area.bottom() - 30); }
        for (int i = 1; i < 6; ++i) { const int y = area.top() + area.height() * i / 6; p.drawLine(area.left() + 24, y, area.right() - 24, y); }
        p.setPen(QPen(QColor("#e6f0fb"), 2)); p.drawPath(body);
        const QRect legend(rect().right() - 45, 80, 13, qMax(120, height() - 175)); QLinearGradient lg(legend.topLeft(), legend.bottomLeft());
        lg.setColorAt(0, QColor("#d94e4e")); lg.setColorAt(.35, QColor("#f6c84b")); lg.setColorAt(.67, QColor("#10a4bd")); lg.setColorAt(1, QColor("#174f88")); p.fillRect(legend, lg); p.setPen(QColor("#c8d7e8")); p.drawRect(legend);
        p.setPen(QColor("#eff6fd")); p.drawText(QRect(20, 14, width() - 50, 30), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"三维结果预览"));
        p.setPen(QColor("#aac2da")); p.drawText(QRect(20, height() - 34, width() - 40, 20), Qt::AlignLeft | Qt::AlignVCenter, uiText(u"翼身组合 / 压力场 / 时间步 0"));
        p.drawText(QRect(0, 0, width() - 19, height()), Qt::AlignRight | Qt::AlignVCenter, uiText(u"压力"));
    }
};
}

PostProcessWorkbenchWindow::PostProcessWorkbenchWindow(QWidget *parent) : QMainWindow(parent) {
    setObjectName(uiText(u"postProcessWorkbench")); setWindowTitle(uiText(u"AeroSimWorkbench - 后处理工作台")); resize(1320, 820); setMinimumSize(1120, 680);
    QFont font(uiText(u"Microsoft YaHei UI")); font.setPointSize(10); setFont(font); createActions();
    auto *workbench = new QWidget(this); auto *root = new QVBoxLayout(workbench); root->setContentsMargins(7, 7, 7, 0); root->setSpacing(6);
    auto *content = new QSplitter(Qt::Horizontal, workbench); content->setObjectName(uiText(u"postProcessContent"));
    auto *tabs = new QTabWidget(content); tabs->setObjectName(uiText(u"postProcessNavigator")); tabs->setMinimumWidth(280); tabs->setMaximumWidth(360); tabs->addTab(createDataPanel(), uiText(u"数据集")); tabs->addTab(createScenePanel(), uiText(u"图层"));
    content->addWidget(tabs); content->addWidget(createViewportPanel()); content->addWidget(createPropertiesPanel()); content->setStretchFactor(1, 1); content->setSizes({300, 830, 320}); content->setChildrenCollapsible(false);
    root->addWidget(content, 1); root->addWidget(createTimeline()); setCentralWidget(workbench);
    auto *dock = new QDockWidget(uiText(u"操作消息"), this); dock->setObjectName(uiText(u"postProcessMessageDock")); events_ = new QTextEdit(dock); events_->setReadOnly(true); dock->setWidget(events_); addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide();
    statusBar()->showMessage(uiText(u"已加载翼身组合结果集：在左侧选择数据，再使用工具栏创建显示或分析对象。"));
    setStyleSheet(uiText(u"QMainWindow{background:#edf2f6;color:#24364a;}QMenuBar,QToolBar{background:#fff;border-bottom:1px solid #ccd7e2;}QMenuBar::item{padding:6px 12px;}QMenuBar::item:selected,QMenu::item:selected{background:#e2effa;color:#0a5d9f;}QMenu::item{padding:7px 30px 7px 12px;}QToolBar{spacing:5px;padding:6px 8px;}QToolButton{min-width:64px;min-height:30px;border:1px solid transparent;border-radius:4px;padding:2px 7px;font-weight:600;}QToolButton:hover{background:#e3eff9;border-color:#bfd5e9;}QToolButton:disabled{color:#9ba9b5;background:#f3f5f7;}QTabWidget::pane,QFrame#viewportFrame,QGroupBox{background:#fff;border:1px solid #ccd7e2;}QTabBar::tab{padding:8px 18px;background:#e7edf3;}QTabBar::tab:selected{background:#fff;color:#0a5d9f;}QTreeWidget,QListWidget{border:0;background:#fff;outline:0;}QTreeWidget::item,QListWidget::item{min-height:27px;padding:4px;}QTreeWidget::item:selected,QListWidget::item:selected{background:#e3eff9;color:#075b9e;}QFrame#dataCard,QFrame#postProcessTimeline{background:#f5f8fc;border:1px solid #d7e2ec;border-radius:5px;}QComboBox,QLineEdit{min-height:27px;border:1px solid #c5d1dd;border-radius:4px;padding:2px 7px;background:#fff;}QPushButton{min-height:28px;border:1px solid #c5d1dd;border-radius:4px;background:#fff;padding:3px 9px;}QPushButton:hover{background:#e3eff9;border-color:#76a8d1;}QPushButton#primaryAction{background:#0a69b7;color:#fff;border:0;font-weight:bold;}QLabel#mutedText{color:#65798d;font-size:12px;}QLabel#sectionTitle{color:#31475f;font-weight:bold;padding:4px 2px;}QGroupBox{margin-top:12px;padding:8px;font-weight:bold;}QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;}QTextEdit{background:#162536;color:#dceafb;border:0;}"));
    updateDataSelection(datasetTree_->currentItem(), 0);
}

void PostProcessWorkbenchWindow::createActions() {
    auto *file = menuBar()->addMenu(uiText(u"文件")); auto *importAction = file->addAction(uiText(u"导入结果…")); auto *exportAction = file->addAction(uiText(u"导出…"));
    auto *data = menuBar()->addMenu(uiText(u"数据")); data->addAction(uiText(u"结果集信息…")); data->addAction(uiText(u"计算字段…")); data->addSeparator(); auto *convertAction = data->addAction(uiText(u"结构网格转非结构网格…")); auto *excelAction = data->addAction(uiText(u"Excel 工况汇总…"));
    auto *display = menuBar()->addMenu(uiText(u"显示")); auto *meshAction = display->addAction(uiText(u"显示网格 / 边线")); auto *contourAction = display->addAction(uiText(u"新建云图")); display->addAction(uiText(u"设置色标与范围…"));
    auto *feature = menuBar()->addMenu(uiText(u"特征")); auto *sliceAction = feature->addAction(uiText(u"添加切面…")); auto *isoAction = feature->addAction(uiText(u"提取等值面…")); auto *streamAction = feature->addAction(uiText(u"生成流线…")); auto *traceAction = feature->addAction(uiText(u"生成轨迹线…"));
    auto *analysis = menuBar()->addMenu(uiText(u"分析")); for (const QString &text : {uiText(u"收敛与监控曲线…"), uiText(u"多工况六分量汇总…"), uiText(u"公式与舵效计算…"), uiText(u"系列线与图表…"), uiText(u"AI 气动力建模与预测…")}) analysis->addAction(text);
    file->addSeparator(); auto *geometryExportAction = file->addAction(uiText(u"导出特征几何…")); auto *animationExportAction = file->addAction(uiText(u"导出动画…"));
    auto *tool = menuBar()->addMenu(uiText(u"工具")); auto *fitAction = tool->addAction(uiText(u"适配视图")); auto *probeAction = tool->addAction(uiText(u"探针查询"));
    auto *bar = addToolBar(uiText(u"后处理操作")); bar->setObjectName(uiText(u"postProcessToolbar")); auto *import = toolButton(bar, uiText(u"导入结果"), uiText(u"导入网格、结果或多时间步数据")); bar->addSeparator();
    auto *mesh = toolButton(bar, uiText(u"显示网格"), uiText(u"切换当前 Zone 的网格和边线")); auto *contour = toolButton(bar, uiText(u"新建云图"), uiText(u"将选中字段以云图方式添加到图层")); bar->addSeparator(); auto *slice = toolButton(bar, uiText(u"添加切面"), uiText(u"在体网格中创建可编辑的切面")); auto *iso = toolButton(bar, uiText(u"提取等值面"), uiText(u"根据选中字段提取三维等值面")); auto *stream = toolButton(bar, uiText(u"生成流线"), uiText(u"从种子点生成流线或表面流线")); auto *trace = toolButton(bar, uiText(u"生成轨迹"), uiText(u"针对时间序结果生成粒子轨迹")); bar->addSeparator(); auto *fit = toolButton(bar, uiText(u"适配视图"), uiText(u"让可见图层适配当前视口")); auto *probe = toolButton(bar, uiText(u"探针"), uiText(u"查询单元、节点的坐标和字段值")); bar->addSeparator(); auto *exportButton = toolButton(bar, uiText(u"导出"), uiText(u"导出图层、几何、图像或动画"));
    mesh->setObjectName(uiText(u"postProcessMeshButton")); contour->setObjectName(uiText(u"postProcessContourButton")); slice->setObjectName(uiText(u"postProcessSliceButton")); iso->setObjectName(uiText(u"postProcessIsoButton")); stream->setObjectName(uiText(u"postProcessStreamButton")); trace->setObjectName(uiText(u"postProcessTraceButton")); featureButtons_ = {mesh, contour, slice, iso, stream, trace};
    const auto command = [this](const QString &key, const QString &message) { emit postProcessCommandRequested(key); appendMessage(message); };
    connect(importAction, &QAction::triggered, this, [command] { command(uiText(u"importResults"), uiText(u"已打开导入结果流程；结果读取服务接入后将填充数据集树。")); }); connect(import, &QToolButton::clicked, importAction, &QAction::trigger);
    connect(exportAction, &QAction::triggered, this, [command] { command(uiText(u"export"), uiText(u"已提交导出请求；请在图层页选择导出对象。")); }); connect(exportButton, &QToolButton::clicked, exportAction, &QAction::trigger);
    connect(convertAction, &QAction::triggered, this, [command] { command(uiText(u"convertStructuredMesh"), uiText(u"已打开结构网格转非结构网格流程。")); }); connect(excelAction, &QAction::triggered, this, [command] { command(uiText(u"aggregateExcelCases"), uiText(u"已打开 Excel 工况汇总流程。")); }); connect(geometryExportAction, &QAction::triggered, this, [command] { command(uiText(u"exportFeatureGeometry"), uiText(u"已打开特征几何导出流程。")); }); connect(animationExportAction, &QAction::triggered, this, [command] { command(uiText(u"exportAnimation"), uiText(u"已打开时间序列动画导出流程。")); });
    connect(meshAction, &QAction::triggered, mesh, &QToolButton::click); connect(mesh, &QToolButton::clicked, this, [command] { command(uiText(u"toggleMesh"), uiText(u"已切换当前 Zone 的网格显示。")); });
    const auto contourFn = [this] { addDerivedLayer(uiText(u"contour"), uiText(u"压力云图"), uiText(u"已将“压力”以云图添加到图层。")); }; connect(contourAction, &QAction::triggered, this, contourFn); connect(contour, &QToolButton::clicked, this, contourFn);
    const auto sliceFn = [this] { addDerivedLayer(uiText(u"slice"), uiText(u"切面 1（X = 0.0）"), uiText(u"已创建切面；可在右侧修改位置与显示字段。")); }; connect(sliceAction, &QAction::triggered, this, sliceFn); connect(slice, &QToolButton::clicked, this, sliceFn);
    const auto isoFn = [this] { addDerivedLayer(uiText(u"iso"), uiText(u"等值面 1（压力）"), uiText(u"已创建等值面；请设置等值以便后续服务执行。")); }; connect(isoAction, &QAction::triggered, this, isoFn); connect(iso, &QToolButton::clicked, this, isoFn);
    const auto streamFn = [this] { addDerivedLayer(uiText(u"stream"), uiText(u"流线 1"), uiText(u"已创建流线对象；请在右侧设置种子和积分方向。")); }; connect(streamAction, &QAction::triggered, this, streamFn); connect(stream, &QToolButton::clicked, this, streamFn);
    const auto traceFn = [this] { addDerivedLayer(uiText(u"trace"), uiText(u"粒子轨迹 1"), uiText(u"已创建轨迹对象；时序数据接入后可运行追踪。")); }; connect(traceAction, &QAction::triggered, this, traceFn); connect(trace, &QToolButton::clicked, this, traceFn);
    connect(fitAction, &QAction::triggered, this, [this] { emit viewRequested(uiText(u"reset")); appendMessage(uiText(u"视图已适配所有可见图层。")); }); connect(fit, &QToolButton::clicked, fitAction, &QAction::trigger); connect(probeAction, &QAction::triggered, this, [command] { command(uiText(u"probe"), uiText(u"已进入探针查询模式：请在视口点选单元或节点。")); }); connect(probe, &QToolButton::clicked, probeAction, &QAction::trigger);
    for (QAction *action : analysis->actions()) connect(action, &QAction::triggered, this, [command, action] { command(uiText(u"openAnalysis"), uiText(u"已打开“%1”工作流程入口。").arg(action->text())); });
}

QWidget *PostProcessWorkbenchWindow::createDataPanel() {
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8); auto *row = new QHBoxLayout; row->addWidget(sectionLabel(uiText(u"当前结果集"), panel)); row->addStretch(); auto *import = new QPushButton(uiText(u"＋ 导入"), panel); import->setObjectName(uiText(u"primaryAction")); row->addWidget(import); layout->addLayout(row);
    auto *card = new QFrame(panel); card->setObjectName(uiText(u"dataCard")); auto *cardLayout = new QVBoxLayout(card); sessionContext_ = new QLabel(uiText(u"翼身组合气动算例 / 结果集"), card); sessionContext_->setStyleSheet(uiText(u"font-weight:600;color:#25364c;")); auto *description = new QLabel(uiText(u"选择 Zone、边界或字段后，工具栏仅点亮当前数据支持的功能。"), card); description->setObjectName(uiText(u"mutedText")); description->setWordWrap(true); cardLayout->addWidget(sessionContext_); cardLayout->addWidget(description); layout->addWidget(card);
    selectionSummary_ = new QLabel(uiText(u"当前选择：流场体网格"), panel); selectionSummary_->setObjectName(uiText(u"mutedText")); layout->addWidget(selectionSummary_); datasetTree_ = new QTreeWidget(panel); datasetTree_->setObjectName(uiText(u"postProcessDatasetTree")); datasetTree_->setHeaderHidden(true); datasetTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *zones = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"■  网格与 Zone"))); zones->setData(0, Qt::UserRole, "group"); auto *volume = new QTreeWidgetItem(zones, QStringList(uiText(u"◆  流场体网格"))); volume->setData(0, Qt::UserRole, "volume"); for (const QString &text : {uiText(u"翼面 Zone"), uiText(u"机身壁面")}) { auto *item = new QTreeWidgetItem(zones, QStringList(uiText(u"◆  ") + text)); item->setData(0, Qt::UserRole, "surface"); }
    auto *fields = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"◉  物理字段"))); fields->setData(0, Qt::UserRole, "group"); for (const QString &text : {uiText(u"压力 [Pa]"), uiText(u"速度大小 [m/s]"), uiText(u"温度 [K]"), uiText(u"马赫数 [-]")}) { auto *item = new QTreeWidgetItem(fields, QStringList(uiText(u"●  ") + text)); item->setData(0, Qt::UserRole, "field"); }
    auto *time = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"◷  时间序列"))); time->setData(0, Qt::UserRole, "time"); for (const QString &text : {uiText(u"t = 0.0 s"), uiText(u"t = 0.5 s"), uiText(u"t = 1.0 s")}) { auto *item = new QTreeWidgetItem(time, QStringList(text)); item->setData(0, Qt::UserRole, "time"); }
    auto *derived = new QTreeWidgetItem(datasetTree_, QStringList(uiText(u"◇  派生对象"))); derived->setData(0, Qt::UserRole, "group"); auto *curve = new QTreeWidgetItem(derived, QStringList(uiText(u"∿  阻力系数监控曲线"))); curve->setData(0, Qt::UserRole, "curve"); datasetTree_->expandAll(); datasetTree_->setCurrentItem(volume); layout->addWidget(datasetTree_, 1); auto *hint = new QLabel(uiText(u"数据集中的结构、字段与时间步由读取服务提供；不依赖主程他处的点击入口。"), panel); hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint); connect(datasetTree_, &QTreeWidget::itemClicked, this, &PostProcessWorkbenchWindow::updateDataSelection); connect(import, &QPushButton::clicked, this, [this] { emit postProcessCommandRequested(uiText(u"importResults")); appendMessage(uiText(u"已打开结果导入流程。")); }); return panel;
}

QWidget *PostProcessWorkbenchWindow::createScenePanel() {
    auto *panel = new QWidget(this); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8); auto *row = new QHBoxLayout; row->addWidget(sectionLabel(uiText(u"显示图层"), panel)); row->addStretch(); auto *add = new QPushButton(uiText(u"＋ 新建"), panel); row->addWidget(add); layout->addLayout(row); auto *hint = new QLabel(uiText(u"图层是独立的显示对象：可以分别显隐、重命名和导出。"), panel); hint->setObjectName(uiText(u"mutedText")); hint->setWordWrap(true); layout->addWidget(hint); sceneList_ = new QListWidget(panel); sceneList_->setObjectName(uiText(u"sceneLayerList")); sceneList_->addItems({uiText(u"◉  体网格和边线  可见"), uiText(u"◉  压力云图      可见")}); sceneList_->setCurrentRow(1); layout->addWidget(sceneList_, 1); auto *actions = new QHBoxLayout; auto *visible = new QPushButton(uiText(u"显隐"), panel); auto *remove = new QPushButton(uiText(u"删除"), panel); actions->addWidget(visible); actions->addWidget(remove); layout->addLayout(actions); connect(add, &QPushButton::clicked, this, [this] { addDerivedLayer(uiText(u"layer"), uiText(u"新显示图层"), uiText(u"已创建新图层，请在右侧配置。")); }); connect(sceneList_, &QListWidget::itemSelectionChanged, this, &PostProcessWorkbenchWindow::updateSelectionDetails); connect(visible, &QPushButton::clicked, this, [this] { appendMessage(uiText(u"已切换选中图层的可见性。")); }); connect(remove, &QPushButton::clicked, this, [this] { if (sceneList_->currentRow() > 1) { delete sceneList_->takeItem(sceneList_->currentRow()); appendMessage(uiText(u"已移除选中图层。")); } else appendMessage(uiText(u"基础网格与云图图层不能在演示数据中移除。")); }); return panel;
}

QWidget *PostProcessWorkbenchWindow::createViewportPanel() { auto *frame = new QFrame(this); frame->setObjectName(uiText(u"viewportFrame")); viewportLayout_ = new QVBoxLayout(frame); viewportLayout_->setContentsMargins(0, 0, 0, 0); emptyViewport_ = new ResultPreviewWidget(frame); emptyViewport_->setObjectName(uiText(u"postProcessResultPreview")); viewportLayout_->addWidget(emptyViewport_); return frame; }

QWidget *PostProcessWorkbenchWindow::createPropertiesPanel() {
    auto *panel = new QWidget(this); panel->setMinimumWidth(290); panel->setMaximumWidth(370); auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(8, 8, 8, 8); layout->addWidget(sectionLabel(uiText(u"选中对象"), panel)); propertiesStack_ = new QStackedWidget(panel); propertiesStack_->setObjectName(uiText(u"postProcessProperties")); auto *none = new QLabel(uiText(u"在数据集或图层中选择对象，以查看其属性与可用操作。"), propertiesStack_); none->setAlignment(Qt::AlignCenter); none->setWordWrap(true); propertiesStack_->addWidget(none); auto *details = new QWidget(propertiesStack_); auto *detailsLayout = new QVBoxLayout(details); propertyContextTitle_ = new QLabel(details); propertyContextTitle_->setStyleSheet(uiText(u"font-size:15px;font-weight:600;color:#234d7a;")); propertyContextHint_ = new QLabel(details); propertyContextHint_->setObjectName(uiText(u"mutedText")); propertyContextHint_->setWordWrap(true); detailsLayout->addWidget(propertyContextTitle_); detailsLayout->addWidget(propertyContextHint_); auto *identity = new QGroupBox(uiText(u"对象"), details); auto *identityForm = new QFormLayout(identity); selectionName_ = new QLineEdit(identity); selectionName_->setReadOnly(true); identityForm->addRow(uiText(u"名称"), selectionName_); auto *show = new QCheckBox(uiText(u"在视口中显示"), identity); show->setChecked(true); identityForm->addRow(show); detailsLayout->addWidget(identity); auto *display = new QGroupBox(uiText(u"显示设置"), details); auto *displayForm = new QFormLayout(display); auto *field = new QComboBox(display); field->addItems({uiText(u"压力"), uiText(u"速度大小"), uiText(u"温度"), uiText(u"马赫数")}); auto *style = new QComboBox(display); style->addItems({uiText(u"云图"), uiText(u"网格线"), uiText(u"实体表面"), uiText(u"边线")}); auto *opacity = new QSlider(Qt::Horizontal, display); opacity->setRange(0,100); opacity->setValue(100); displayForm->addRow(uiText(u"字段"),field); displayForm->addRow(uiText(u"显示方式"),style); displayForm->addRow(uiText(u"不透明度"),opacity); detailsLayout->addWidget(display); auto *parameters = new QGroupBox(uiText(u"特征与分析"), details); auto *form = new QFormLayout(parameters); auto *parameter = new QLineEdit(uiText(u"根据当前对象配置"), parameters); parameter->setReadOnly(true); form->addRow(uiText(u"参数"), parameter); detailsLayout->addWidget(parameters); detailsLayout->addStretch(); propertiesStack_->addWidget(details); layout->addWidget(propertiesStack_,1); return panel;
}

QWidget *PostProcessWorkbenchWindow::createTimeline() { auto *frame = new QFrame(this); frame->setObjectName(uiText(u"postProcessTimeline")); auto *layout = new QHBoxLayout(frame); layout->setContentsMargins(8,5,8,5); layout->addWidget(new QLabel(uiText(u"时间步"),frame)); for(const QString &text:{uiText(u"|◀"),uiText(u"◀"),uiText(u"▶"),uiText(u"▶|")}){auto *button=new QPushButton(text,frame);button->setFixedWidth(34);layout->addWidget(button);connect(button,&QPushButton::clicked,this,[this]{appendMessage(uiText(u"已切换时间步。"));});} timeSlider_=new QSlider(Qt::Horizontal,frame);timeSlider_->setObjectName(uiText(u"postProcessTimeSlider"));timeSlider_->setRange(0,2);layout->addWidget(timeSlider_,1);timeLabel_=new QLabel(uiText(u"t = 0.0 s / 3 帧"),frame);layout->addWidget(timeLabel_);connect(timeSlider_,&QSlider::valueChanged,this,[this](int value){timeLabel_->setText(uiText(u"t = %1 s / 3 帧").arg(value*.5,0,'f',1));});return frame; }

void PostProcessWorkbenchWindow::setFeatureButtonsEnabled(bool mesh, bool contour, bool slice, bool iso, bool streamline) { const bool states[]={mesh,contour,slice,iso,streamline,streamline}; for(int i=0;i<featureButtons_.size();++i){featureButtons_.at(i)->setEnabled(states[i]);featureButtons_.at(i)->setToolTip(states[i]?featureButtons_.at(i)->property("availableToolTip").toString():uiText(u"当前选中数据不支持此功能。请选择体网格或相关字段。"));} }
void PostProcessWorkbenchWindow::updateDataSelection(QTreeWidgetItem *item, int) { if(!item)return; const QString type=item->data(0,Qt::UserRole).toString(); const QString name=item->text(0).mid(3).trimmed(); selectionSummary_->setText(uiText(u"当前选择：")+name);selectionName_->setText(name);propertiesStack_->setCurrentIndex(1);propertyContextTitle_->setText(name);const bool spatial=type=="volume"||type=="surface"||type=="field"||type=="time";const bool field=type=="field";propertyContextHint_->setText(field?uiText(u"可将此字段用于云图、切面和等值面。"):type=="curve"?uiText(u"此对象适合用于曲线与多工况分析。"):uiText(u"可对当前对象创建或编辑后处理图层。"));setFeatureButtonsEnabled(spatial,spatial,type=="volume"||field||type=="time",type=="volume"||field,type=="volume"||type=="surface"||field||type=="time"); }
void PostProcessWorkbenchWindow::addDerivedLayer(const QString &kind,const QString &name,const QString &message){emit postProcessCommandRequested(kind);if(sceneList_){sceneList_->addItem(uiText(u"◉  ")+name+uiText(u"  可见"));sceneList_->setCurrentRow(sceneList_->count()-1);}appendMessage(message);}
bool PostProcessWorkbenchWindow::insertWidget(WidgetSlot slot,QWidget *widget){if(slot!=WidgetSlot::Viewport||!widget||!viewportLayout_)return false;if(emptyViewport_){viewportLayout_->removeWidget(emptyViewport_);emptyViewport_->deleteLater();emptyViewport_=nullptr;}if(viewportLayout_->count()!=0)return false;viewportLayout_->addWidget(widget);return true;}
void PostProcessWorkbenchWindow::bindServices(const ServiceBindings &services){if(bound_)return;services_=services;bound_=true;if(services_.tasks)connect(services_.tasks,&ITaskService::taskEventOccurred,this,[this](const TaskEvent &event){appendMessage(event.message);},Qt::QueuedConnection);}
void PostProcessWorkbenchWindow::appendMessage(const QString &message){events_->append(message.toHtmlEscaped());statusBar()->showMessage(message,5000);}
void PostProcessWorkbenchWindow::updateSelectionDetails(){const auto selected=sceneList_->selectedItems();if(selected.isEmpty()){propertiesStack_->setCurrentIndex(0);return;}const QString name=selected.first()->text().mid(3).trimmed();selectionName_->setText(name);propertyContextTitle_->setText(uiText(u"显示图层"));propertyContextHint_->setText(uiText(u"可在此调整字段、显示方式、透明度与特征参数。"));propertiesStack_->setCurrentIndex(1);}
} // namespace aerosim
