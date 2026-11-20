#include "aerosim/ui/MeshManagementPage.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QIcon>
#include <QItemSelectionModel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QQuaternion>
#include <QRadioButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <utility>

namespace aerosim {
namespace {
template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

QAction *addViewAction(QToolBar *toolbar, const QString &name, const QString &label,
                       const QIcon &icon, bool enabled = true)
{
    auto *action = toolbar->addAction(icon, label);
    action->setObjectName(name);
    action->setEnabled(enabled);
    if (auto *widget = toolbar->widgetForAction(action)) {
        widget->setObjectName(name);
        widget->setAccessibleName(label);
    }
    return action;
}

QIcon cubeIcon(int face, bool edges = false)
{
    QPixmap pixmap(48, 48);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor ink(QStringLiteral("#1c193c"));
    painter.setPen(QPen(ink, 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPolygonF front = QPolygonF() << QPointF(3, 8) << QPointF(16, 8)
                                        << QPointF(16, 22) << QPointF(3, 22);
    const QPolygonF top = QPolygonF() << QPointF(3, 8) << QPointF(9, 2)
                                      << QPointF(22, 2) << QPointF(16, 8);
    const QPolygonF side = QPolygonF() << QPointF(16, 8) << QPointF(22, 2)
                                       << QPointF(22, 16) << QPointF(16, 22);
    const std::array<QPolygonF, 3> faces = {{front, side, top}};
    for (int i = 0; i < int(faces.size()); ++i) {
        painter.setBrush(face == i ? ink : face == 6 ? QColor("#d8d6e5") : QColor(Qt::white));
        painter.drawPolygon(faces[i]);
    }
    if (face >= 3 && face <= 5) {
        const std::array<QPointF, 3> corners = {{QPointF(3, 22), QPointF(9, 2), QPointF(22, 16)}};
        painter.setPen(QPen(ink, 1.2, Qt::DashLine));
        for (const auto &corner : corners)
            painter.drawLine(QPointF(9, 16), corner);
        painter.setPen(QPen(ink, 2.0));
        painter.drawLine(QPointF(9, 16), corners[face - 3]);
    } else if (edges) {
        painter.setPen(QPen(ink, 0.8));
        painter.drawLine(QPointF(3, 15), QPointF(16, 15));
        painter.drawLine(QPointF(9.5, 8), QPointF(9.5, 22));
        painter.drawLine(QPointF(16, 15), QPointF(22, 9));
    }
    painter.end();
    return QIcon(pixmap);
}

QIcon toolbarIcon(const QString &name)
{
    QPixmap pixmap = QIcon(QStringLiteral(":/aerosim/ribbon/%1.svg").arg(name)).pixmap(48, 48);
    pixmap.setDevicePixelRatio(2.0);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(QRect(0, 0, 24, 24), QColor("#1c193c"));
    painter.end();
    return QIcon(pixmap);
}

QString meshCanvasBackground(int mode)
{
    const QString gradient = mode == 1
        ? QStringLiteral("stop:0 #2f3b49,stop:1 #4a5a6d")
        : mode == 2
        ? QStringLiteral("stop:0 #565c77,stop:1 #ffffff")
        : QStringLiteral("stop:0 #b8a9e3,stop:1 #f8f5fc");
    return QStringLiteral("#meshViewerCanvas { background:qlineargradient("
                          "x1:0,y1:0,x2:0,y2:1,%1); }").arg(gradient);
}

// Project the six signed world axes into camera space. Drawing far endpoints
// first leaves the facing axis labelled when it overlaps the opposite end.
const std::array<QVector3D, 3> worldAxes = {{QVector3D(1, 0, 0), QVector3D(0, 1, 0), QVector3D(0, 0, 1)}};

class AxisTriad final : public QWidget {
public:
    AxisTriad(QWidget *parent, std::function<void(int, int)> orbit,
              std::function<void(const QString &)> snap)
        : QWidget(parent), orbit_(std::move(orbit)), snap_(std::move(snap))
    {
        setObjectName(QStringLiteral("axisIndicator"));
        setFixedSize(88, 88);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAccessibleName(QString::fromUtf8("三维方向球 XYZ"));
    }

    void enableInteraction()
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QString::fromUtf8("拖动可旋转视角，点击轴端点可切换到相应视图。"));
    }

    void setView(const QVector3D &from, const QVector3D &up)
    {
        worldToView_ = QQuaternion::fromDirection(from, up).conjugated();
        update();
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) return QWidget::mousePressEvent(event);
        const auto offset = QPointF(event->pos()) - center();
        if (QPointF::dotProduct(offset, offset) > 44 * 44) return QWidget::mousePressEvent(event);
        pressed_ = true;
        dragging_ = false;
        lastPos_ = pressPos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!pressed_) return QWidget::mouseMoveEvent(event);
        if (!dragging_ && (event->pos() - pressPos_).manhattanLength() < QApplication::startDragDistance())
            return;
        dragging_ = true;
        const auto delta = event->pos() - lastPos_;
        lastPos_ = event->pos();
        orbit_(delta.x(), delta.y());
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (!pressed_ || event->button() != Qt::LeftButton) return QWidget::mouseReleaseEvent(event);
        pressed_ = false;
        setCursor(Qt::OpenHandCursor);
        if (!dragging_) {
            const auto points = endpoints();
            for (auto end = points.crbegin(); end != points.crend(); ++end) {
                const auto offset = QPointF(event->pos()) - end->tip;
                if (QPointF::dotProduct(offset, offset) <= end->radius * end->radius) {
                    snap_(QString::fromLatin1(end->command));
                    break;
                }
            }
        }
        event->accept();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const std::array<QColor, 3> colors = {{QColor("#c84437"), QColor("#e7b827"), QColor("#a5cd8a")}};
        const auto points = endpoints();
        QFont labelFont = font();
        labelFont.setBold(true);
        labelFont.setPixelSize(12);
        painter.setFont(labelFont);
        for (const auto &end : points) {
            QColor lineColor = colors[end.axis];
            lineColor.setAlpha(105);
            painter.setPen(QPen(lineColor, 1.6, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(center(), end.tip);
        }
        for (const auto &end : points) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(end.positive ? colors[end.axis] : colors[end.axis].darker(120));
            painter.drawEllipse(end.tip, end.radius, end.radius);
            if (end.positive || end.depth > 0.75f) {
                painter.setPen(QColor("#121212"));
                painter.drawText(QRectF(end.tip.x() - 9, end.tip.y() - 9, 18, 18),
                                 Qt::AlignCenter, (end.positive ? QString() : QStringLiteral("-"))
                                     + QStringLiteral("XYZ").mid(end.axis, 1));
            }
        }
    }

private:
    struct Endpoint {
        QPointF tip;
        qreal radius;
        float depth;
        int axis;
        bool positive;
        const char *command;
    };

    QPointF center() const { return QPointF(width() * 0.5, height() * 0.5); }

    std::array<Endpoint, 6> endpoints() const
    {
        const std::array<const char *, 6> commands = {{"left", "right", "front", "back", "bottom", "top"}};
        const qreal radius = qMin(width(), height()) * 0.38;
        std::array<Endpoint, 6> points;
        for (int i = 0; i < int(points.size()); ++i) {
            const bool positive = i % 2 != 0;
            const auto direction = worldToView_.rotatedVector(positive ? worldAxes[i / 2] : -worldAxes[i / 2]);
            points[i] = {center() + QPointF(direction.x(), -direction.y()) * radius,
                         direction.z() > 0.75f ? 9.5 : 8.0, direction.z(), i / 2, positive, commands[i]};
        }
        std::stable_sort(points.begin(), points.end(), [](const Endpoint &a, const Endpoint &b) {
            return a.depth < b.depth;
        });
        return points;
    }

    std::function<void(int, int)> orbit_;
    std::function<void(const QString &)> snap_;
    QQuaternion worldToView_;
    QPoint pressPos_;
    QPoint lastPos_;
    bool pressed_ = false;
    bool dragging_ = false;
};

class MeshCanvas final : public QFrame {
public:
    explicit MeshCanvas(QWidget *parent) : QFrame(parent) {}

    void setOrientationControl(QWidget *control)
    {
        control_ = control;
        positionControl();
        control_->raise();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QFrame::resizeEvent(event);
        if (control_) positionControl();
    }

private:
    void positionControl()
    {
        control_->move(9, qMax(0, height() - control_->height() - 16));
    }

    QWidget *control_ = nullptr;
};
}

MeshManagementPage::MeshManagementPage(QWidget *viewportHost, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("viewportPage"));
    setStyleSheet(QStringLiteral(
        "#meshManagementHeader { background:#edf3fa; border-bottom:1px solid #c8d5e4; }"
        "#meshManagementTitle { color:#1f3b5b; font-size:15px; font-weight:600; }"
        "#meshInfoPanel { background:#ffffff; border:1px solid #cbd5df; }"
        "#meshInfoTitle { color:#1f4e9c; font-size:14px; font-weight:600; }"
        "#meshHistoryPanel { background:#ffffff; border:1px solid #cbd5df; }"
        "#meshHistoryHeader { background:#2b285f; }"
        "#meshHistoryHeader QLabel { color:#ffffff; }"
        "#meshHistoryCollapseButton { color:#f0effb; background:transparent; border:0; padding:2px 5px; }"
        "#meshHistoryCollapseButton:hover { background:#49477c; }"
        "QSplitter::handle { background:#dde5ee; }"
        "QSplitter::handle:horizontal { width:5px; }"
        "QSplitter::handle:vertical { height:5px; }"
        "QSplitter::handle:hover { background:#b9cde4; }"
        "#meshViewerPanel { background:#ffffff; border:0; }"
        "#meshViewerToolbar { background:#ffffff; border:0; }"
        "#meshViewerToolbar QToolButton { border:0; border-radius:0; padding:0; }"
        "#meshViewerToolbar QToolButton:hover, #meshViewerToolbar QToolButton:focus { background:#e3e5f7; }"
        "#meshViewerToolbar QToolButton:checked { background:#edeef9; }"
        "#meshViewerToolbar QToolButton::menu-indicator { image:none; }"
        "QWidget#viewportSlot { background:transparent; }"
        "#meshTable { border:1px solid #d5dde6; background:#ffffff; gridline-color:#e4eaf1; "
        "alternate-background-color:#f7f9fc; selection-background-color:#dceafd; selection-color:#25364c; }"
        "#meshTable QHeaderView::section { background:#eef3f9; color:#25364c; border:0; "
        "border-right:1px solid #d5dde6; border-bottom:1px solid #d5dde6; padding:4px 6px; }"
        "#meshScopeLabel, #meshCountLabel, #meshViewerStatus { color:#637083; }"
        "#meshViewerStatus { background:#f5f7fa; border-top:1px solid #d5dde6; }"
        "#meshWorkspaceTabs, #meshWorkspaceTabs QTabBar { background:#2b285f; }"
        "#meshWorkspaceTabs::pane { border:0; background:#ffffff; }"
        "#meshWorkspaceTabs QTabBar::tab { background:#2b285f; color:#ffffff; "
        "padding:6px 13px; margin:0; border:0; }"
        "#meshWorkspaceTabs QTabBar::tab:hover:!selected { background:#47447c; }"
        "#meshWorkspaceTabs QTabBar::tab:selected { background:#ffffff; color:#16132f; }"));

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(8, 8, 8, 8);
    pageLayout->setSpacing(6);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("meshManagementHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 7, 12, 7);
    auto *title = new QLabel(uiText(u"\u7f51\u683c\u7ba1\u7406"), header);
    title->setObjectName(QStringLiteral("meshManagementTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    auto *scope = new QLabel(uiText(u"\u524d\u7aef Mock \u4ea4\u4e92 \u00b7 \u672a\u8fde\u63a5\u771f\u5b9e\u7f51\u683c\u6570\u636e"), header);
    scope->setObjectName(QStringLiteral("meshScopeLabel"));
    headerLayout->addWidget(scope);
    pageLayout->addWidget(header);

    // The reference places the log below the viewport in the right column.
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);
    mainSplitter_->setObjectName(QStringLiteral("meshWorkspaceMainSplitter"));
    mainSplitter_->setChildrenCollapsible(false);
    settingsSplitter_ = new QSplitter(Qt::Vertical, mainSplitter_);
    settingsSplitter_->setObjectName(QStringLiteral("meshSettingsSplitter"));
    settingsSplitter_->setChildrenCollapsible(false);

    // Reference "网格信息" panel: coordinate system, mesh table, import/delete.
    auto *objectPanel = new QFrame(mainSplitter_);
    objectPanel->setObjectName(QStringLiteral("meshInfoPanel"));
    objectPanel->setMinimumWidth(245);
    auto *objectLayout = new QVBoxLayout(objectPanel);
    objectLayout->setContentsMargins(10, 8, 10, 10);
    objectLayout->setSpacing(7);

    auto *infoTitle = new QLabel(uiText(u"\u7f51\u683c\u4fe1\u606f"), objectPanel);
    infoTitle->setObjectName(QStringLiteral("meshInfoTitle"));
    infoTitle->setAlignment(Qt::AlignCenter);
    objectLayout->addWidget(infoTitle);

    auto *coordinateRow = new QWidget(objectPanel);
    auto *coordinateLayout = new QHBoxLayout(coordinateRow);
    coordinateLayout->setContentsMargins(0, 0, 0, 0);
    coordinateLayout->setSpacing(8);
    auto *coordinateLabel = new QLabel(uiText(u"\u7f51\u683c\u5750\u6807\u7cfb\uff1a"), coordinateRow);
    solidModelRadio_ = new QRadioButton(uiText(u"\u4e09\u7ef4\u6a21\u578b"), coordinateRow);
    solidModelRadio_->setObjectName(QStringLiteral("meshCoordinateSystemSolid"));
    solidModelRadio_->setChecked(true);
    solidModelRadio_->setToolTip(uiText(u"\u7f51\u683c\u6309\u4e09\u7ef4\u6a21\u578b\u8fdb\u884c\u8fb9\u754c\u63d0\u53d6\u4e0e\u663e\u793a"));
    planarModelRadio_ = new QRadioButton(uiText(u"\u4e8c\u7ef4\u6a21\u578b"), coordinateRow);
    planarModelRadio_->setObjectName(QStringLiteral("meshCoordinateSystemPlanar"));
    planarModelRadio_->setToolTip(uiText(u"\u4e8c\u7ef4\u6a21\u578b\u76ee\u524d\u4ec5\u4fdd\u7559\u9009\u9879\uff0c\u4e0d\u6267\u884c\u5e73\u9762\u63d0\u53d6"));
    coordinateLayout->addWidget(coordinateLabel);
    coordinateLayout->addWidget(solidModelRadio_);
    coordinateLayout->addWidget(planarModelRadio_);
    coordinateLayout->addStretch();
    objectLayout->addWidget(coordinateRow);

    meshTable_ = new QTableWidget(objectPanel);
    meshTable_->setObjectName(QStringLiteral("meshTable"));
    meshTable_->setColumnCount(3);
    meshTable_->setHorizontalHeaderLabels({uiText(u"\u5e8f\u53f7"), uiText(u"\u7f51\u683c\u540d\u79f0"),
                                           uiText(u"\u662f\u5426\u53ef\u89c1")});
    meshTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    meshTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    meshTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    meshTable_->verticalHeader()->hide();
    meshTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    meshTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    meshTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    meshTable_->setAlternatingRowColors(true);
    meshTable_->setMinimumHeight(118);
    objectLayout->addWidget(meshTable_, 1);

    auto *listActions = new QHBoxLayout;
    meshCount_ = new QLabel(objectPanel);
    meshCount_->setObjectName(QStringLiteral("meshObjectCount"));
    listActions->addWidget(meshCount_);
    listActions->addStretch();
    importMeshButton_ = new QPushButton(uiText(u"\u5bfc\u5165"), objectPanel);
    importMeshButton_->setObjectName(QStringLiteral("importMeshButton"));
    importMeshButton_->setToolTip(uiText(u"\u5bfc\u5165\u4e00\u4e2a\u524d\u7aef Mock \u7f51\u683c\uff0c\u6700\u591a 10 \u4e2a"));
    deleteMeshButton_ = new QPushButton(uiText(u"\u5220\u9664"), objectPanel);
    deleteMeshButton_->setObjectName(QStringLiteral("deleteMeshButton"));
    deleteMeshButton_->setToolTip(uiText(u"\u5220\u9664\u5f53\u524d\u9009\u4e2d\u7684\u7f51\u683c\u5bf9\u8c61"));
    listActions->addWidget(importMeshButton_);
    listActions->addWidget(deleteMeshButton_);
    objectLayout->addLayout(listActions);

    // The preprocessing workspace contains only the mesh view.
    workspaceTabs_ = new QTabWidget(settingsSplitter_);
    workspaceTabs_->setObjectName(QStringLiteral("meshWorkspaceTabs"));
    workspaceTabs_->setDocumentMode(false);
    workspaceTabs_->setAttribute(Qt::WA_StyledBackground);

    auto *viewerPanel = new QFrame(workspaceTabs_);
    viewerPanel->setObjectName(QStringLiteral("meshViewerPanel"));
    auto *viewerLayout = new QVBoxLayout(viewerPanel);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);
    viewportWindow_ = new QMainWindow(viewerPanel);
    viewportWindow_->setWindowFlags(Qt::Widget);
    viewportWindow_->setObjectName(QStringLiteral("meshViewportWindow"));
    viewportWindow_->setContextMenuPolicy(Qt::PreventContextMenu);
    viewerLayout->addWidget(viewportWindow_, 1);

    auto *canvas = new MeshCanvas(viewportWindow_);
    canvas->setObjectName(QStringLiteral("meshViewerCanvas"));
    canvas->setStyleSheet(meshCanvasBackground(0));
    auto *canvasLayout = new QGridLayout(canvas);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    viewportHost->setObjectName(QStringLiteral("viewportSlot"));
    if (auto *placeholder = viewportHost->findChild<QLabel *>(QString(), Qt::FindDirectChildrenOnly))
        placeholder->hide();
    canvasLayout->addWidget(viewportHost, 0, 0);

    auto *axis = new AxisTriad(canvas,
        [this](int dx, int dy) { emit boundaryOrbitRequested(dx, dy); },
        [this](const QString &command) { emit boundaryViewRequested(command); });
    axisIndicator_ = axis;
    canvas->setOrientationControl(axis);
    viewportWindow_->setCentralWidget(canvas);

    viewToolbar_ = new QToolBar(QString::fromUtf8("网格视图工具栏"), viewportWindow_);
    auto *toolbar = viewToolbar_;
    toolbar->setObjectName(QStringLiteral("meshViewerToolbar"));
    toolbar->setIconSize(QSize(24, 24));
    toolbar->setAllowedAreas(Qt::AllToolBarAreas);
    toolbar->setMovable(true);
    toolbar->setFloatable(false);
    toolbar->setToolTip(QString::fromUtf8("拖动工具栏手柄可停靠到视口上、下、左、右"));
    viewportWindow_->addToolBar(Qt::RightToolBarArea, toolbar);

    struct PresetView {
        const char *command;
        const char *label;
        QVector3D from;
        QVector3D up;
    };
    const std::array<PresetView, 6> presets = {{
        {"front", "正视（−Y）", {0, -1, 0}, {0, 0, 1}},
        {"back", "后视（+Y）", {0, 1, 0}, {0, 0, 1}},
        {"right", "右视（+X）", {1, 0, 0}, {0, 0, 1}},
        {"left", "左视（−X）", {-1, 0, 0}, {0, 0, 1}},
        {"top", "顶视（+Z）", {0, 0, 1}, {0, 1, 0}},
        {"bottom", "底视（−Z）", {0, 0, -1}, {0, 1, 0}},
    }};
    const auto addPreset = [&](const PresetView &view, const QIcon &icon) {
        const QString command = QString::fromLatin1(view.command);
        auto *action = addViewAction(toolbar, QStringLiteral("boundaryView_") + command,
                                     QString::fromUtf8(view.label), icon);
        connect(action, &QAction::triggered, this, [this, axis, view, command] {
            if (boundaryMode_) emit boundaryViewRequested(command);
            else axis->setView(view.from, view.up);
        });
    };
    const std::array<int, 6> faces = {{0, 3, 1, 4, 2, 5}};
    for (int i = 0; i < int(presets.size()); ++i)
        addPreset(presets[i], cubeIcon(faces[i]));
    addPreset({"iso", "等轴测", {1, -1, 1}, {0, 0, 1}}, cubeIcon(6));

    const auto addRenderCommand = [&](const QString &command, const QString &label, const QIcon &icon) {
        auto *action = addViewAction(toolbar, QStringLiteral("boundaryView_") + command,
                                     label, icon, false);
        action->setProperty("boundaryCommand", command);
        connect(action, &QAction::triggered, this, [this, command] {
            if (boundaryMode_) emit boundaryViewRequested(command);
        });
    };
    addRenderCommand(QStringLiteral("in"), QString::fromUtf8("放大"),
                     toolbarIcon(QStringLiteral("view-zoom-in")));
    addRenderCommand(QStringLiteral("fit"), QString::fromUtf8("适合大小"),
                     toolbarIcon(QStringLiteral("view-fit")));
    toolbar->addSeparator();

    auto *displayModes = new QActionGroup(toolbar);
    displayModes->setExclusive(true);
    const auto addDisplayMode = [&](const QString &name, const QString &label, int mode, const QIcon &icon) {
        auto *action = addViewAction(toolbar, name, label, icon);
        action->setCheckable(true);
        displayModes->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] {
            displayModeChosen_ = true;
            emit boundaryDisplayModeChanged(mode);
        });
        return action;
    };
    auto *edges = addDisplayMode(QStringLiteral("meshSurfaceEdgesButton"), QString::fromUtf8("表面 + 边线"), 2, cubeIcon(6, true));
    surfaceAction_ = addDisplayMode(QStringLiteral("meshSurfaceButton"), QString::fromUtf8("表面"), 0, cubeIcon(6));
    auto *wire = addDisplayMode(QStringLiteral("meshWireframeButton"), QString::fromUtf8("线框"), 1, cubeIcon(-1));
    edges->setChecked(true);
    // Route VTK's display shortcuts through the same viewport-wide actions.
    surfaceAction_->setShortcut(QKeySequence(Qt::Key_S));
    wire->setShortcut(QKeySequence(Qt::Key_W));
    for (auto *action : {surfaceAction_, wire}) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        viewportWindow_->addAction(action);
    }
    addRenderCommand(QStringLiteral("out"), QString::fromUtf8("缩小"),
                     toolbarIcon(QStringLiteral("view-zoom-out")));
    toolbar->addSeparator();

    auto *axisAction = addViewAction(toolbar, QStringLiteral("meshAxisButton"),
        QString::fromUtf8("显示坐标轴"), toolbarIcon(QStringLiteral("view-rotate")));
    axisAction->setCheckable(true);
    axisAction->setChecked(true);
    connect(axisAction, &QAction::toggled, axis, &QWidget::setVisible);
    addPreset({"reset", "重置视图", {0, 0, 1}, {0, 1, 0}},
              toolbarIcon(QStringLiteral("view-front")));
    toolbar->addSeparator();
    addRenderCommand(QStringLiteral("center"), QString::fromUtf8("旋转中心"),
                     toolbarIcon(QStringLiteral("workbench")));

    auto *backgroundAction = addViewAction(toolbar, QStringLiteral("meshBackgroundMode"),
        QString::fromUtf8("视口背景"), toolbarIcon(QStringLiteral("settings")));
    auto *backgroundMenu = new QMenu(toolbar);
    auto *backgroundModes = new QActionGroup(backgroundMenu);
    backgroundModes->setExclusive(true);
    const QStringList backgroundLabels = {QString::fromUtf8("淡紫渐变"), QString::fromUtf8("深色"),
                                          QString::fromUtf8("灰白渐变")};
    for (int i = 0; i < backgroundLabels.size(); ++i) {
        auto *action = backgroundMenu->addAction(backgroundLabels.at(i));
        action->setCheckable(true);
        backgroundModes->addAction(action);
        action->setChecked(i == 0);
        connect(action, &QAction::triggered, this, [this, canvas, i] {
            canvas->setProperty("backgroundMode", i);
            canvas->setStyleSheet(meshCanvasBackground(i));
            if (boundaryMode_) emit boundaryBackgroundRequested(i);
        });
    }
    backgroundAction->setMenu(backgroundMenu);
    if (auto *button = qobject_cast<QToolButton *>(toolbar->widgetForAction(backgroundAction)))
        button->setPopupMode(QToolButton::InstantPopup);

    viewerStatus_ = new QLabel(viewerPanel);
    viewerStatus_->setObjectName(QStringLiteral("meshViewerStatus"));
    viewerStatus_->setContentsMargins(10, 4, 10, 4);
    viewerStatus_->hide();
    viewerLayout->addWidget(viewerStatus_);
    workspaceTabs_->addTab(viewerPanel, uiText(u"\u7f51\u683c\u89c6\u56fe"));

    settingsSplitter_->addWidget(workspaceTabs_);
    settingsSplitter_->setStretchFactor(0, 1);

    // Keep the log available below the right-hand view, collapsed on entry.
    auto *historyPanel = new QFrame(settingsSplitter_);
    historyPanel->setObjectName(QStringLiteral("meshHistoryPanel"));
    auto *historyLayout = new QVBoxLayout(historyPanel);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(0);
    historyHeader_ = new QWidget(historyPanel);
    historyHeader_->setObjectName(QStringLiteral("meshHistoryHeader"));
    auto *historyHeaderLayout = new QHBoxLayout(historyHeader_);
    historyHeaderLayout->setContentsMargins(9, 3, 7, 3);
    auto *historyTitle = new QLabel(uiText(u"\u65e5\u5fd7"), historyHeader_);
    QFont historyTitleFont = historyTitle->font();
    historyTitleFont.setBold(false);
    historyTitle->setFont(historyTitleFont);
    historyHeaderLayout->addWidget(historyTitle);
    historyHeaderLayout->addStretch();
    historyCollapseButton_ = new QPushButton(uiText(u"\u6536\u8d77"), historyHeader_);
    historyCollapseButton_->setObjectName(QStringLiteral("meshHistoryCollapseButton"));
    historyCollapseButton_->setMaximumWidth(58);
    historyHeaderLayout->addWidget(historyCollapseButton_);
    historyLayout->addWidget(historyHeader_);
    historyTabs_ = new QTabWidget(historyPanel);
    historyTabs_->setObjectName(QStringLiteral("meshHistoryTabs"));
    historyList_ = new QListWidget(historyTabs_);
    historyList_->setObjectName(QStringLiteral("meshOperationHistory"));
    historyList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // The reference log uses the regular UI font and wraps its lines instead
    // of scrolling horizontally, so long extraction summaries stay readable at
    // the narrow settings-column width.
    historyList_->setWordWrap(true);
    historyList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    historyList_->setTextElideMode(Qt::ElideNone);
    historyTabs_->addTab(historyList_, uiText(u"\u64cd\u4f5c\u8bb0\u5f55"));
    auto *scriptPage = new QWidget(historyTabs_);
    auto *scriptLayout = new QVBoxLayout(scriptPage);
    auto *scriptNote = new QLabel(uiText(u"\u811a\u672c\u63a7\u5236\u4e3a\u540e\u7eed\u7248\u672c\u9884\u7559\uff1b\u672c\u671f\u4e0d\u542f\u52a8 Python \u89e3\u91ca\u5668\u6216\u5916\u90e8\u8fdb\u7a0b\u3002"), scriptPage);
    scriptNote->setWordWrap(true);
    auto *scriptInput = new QPlainTextEdit(scriptPage);
    scriptInput->setObjectName(QStringLiteral("meshScriptPlaceholder"));
    scriptInput->setPlaceholderText(uiText(u"# \u540e\u7eed\u7248\u672c\u652f\u6301\u7f51\u683c\u811a\u672c\u63a7\u5236"));
    scriptInput->setEnabled(false);
    scriptLayout->addWidget(scriptNote);
    scriptLayout->addWidget(scriptInput, 1);
    historyTabs_->addTab(scriptPage, uiText(u"\u811a\u672c\u63a7\u5236\uff08\u9884\u7559\uff09"));
    historyLayout->addWidget(historyTabs_, 1);
    settingsSplitter_->addWidget(historyPanel);
    settingsSplitter_->setStretchFactor(1, 0);

    mainSplitter_->addWidget(objectPanel);
    mainSplitter_->addWidget(settingsSplitter_);
    mainSplitter_->setStretchFactor(0, 1);
    mainSplitter_->setStretchFactor(1, 1);
    mainSplitter_->setSizes({670, 330});
    pageLayout->addWidget(mainSplitter_, 1);

    connect(importMeshButton_, &QPushButton::clicked, this, &MeshManagementPage::addMesh);
    connect(deleteMeshButton_, &QPushButton::clicked, this, &MeshManagementPage::removeCurrentMesh);
    connect(meshTable_, &QTableWidget::currentCellChanged, this,
        [this](int row, int, int, int) { selectMesh(row); });
    connect(meshTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (!item || item->column() != 2) return;
        const int row = item->row();
        if (row < 0 || row >= meshes_.size() || !meshes_[row].ready) return;
        meshes_[row].visible = item->checkState() == Qt::Checked;
        if (boundaryMode_) emit boundaryVisibilityChanged(meshes_[row].id, meshes_[row].visible);
        appendHistory((meshes_[row].visible ? uiText(u"\u663e\u793a ") : uiText(u"\u9690\u85c5 ")) + meshes_[row].name);
    });
    connect(historyCollapseButton_, &QPushButton::clicked, this,
        [this] { setHistoryCollapsed(!historyCollapsed_); });

    for (int i = 0; i < 3; ++i) {
        MeshUiState state;
        state.name = QStringLiteral("Mesh_%1").arg(i + 1, 3, 10, QLatin1Char('0'));
        state.visible = i != 2;
        meshes_.append(state);
        meshTable_->insertRow(meshTable_->rowCount());
        {
            // Assigning a checkable item emits itemChanged while its check
            // state is still unset; without this guard that would be read as
            // the user hiding the mesh.
            const QSignalBlocker block(meshTable_);
            auto *indexItem = new QTableWidgetItem;
            indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
            auto *nameItem = new QTableWidgetItem;
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            auto *visibleItem = new QTableWidgetItem;
            visibleItem->setFlags(visibleItem->flags() | Qt::ItemIsUserCheckable);
            visibleItem->setTextAlignment(Qt::AlignCenter);
            meshTable_->setItem(i, 0, indexItem);
            meshTable_->setItem(i, 1, nameItem);
            meshTable_->setItem(i, 2, visibleItem);

        }
        refreshTableRow(i);
    }
    updateCount();
    appendHistory(uiText(u"\u521d\u59cb\u5316 3 \u4e2a Mock \u7f51\u683c\u5bf9\u8c61"));
    meshTable_->setCurrentCell(0, 1);
    setHistoryCollapsed(true);
}

void MeshManagementPage::addMesh()
{
    if (boundaryMode_) {
        QString filter = QString::fromUtf8("VTK 网格 (*.vtk *.vtu *.vti *.vtp)");
        if (cgnsAvailable_) filter.prepend(QString::fromUtf8("网格 (*.cgns *.vtk *.vtu *.vti *.vtp);;CGNS 网格 (*.cgns);;"));
        const auto paths = QFileDialog::getOpenFileNames(this, QString::fromUtf8("导入网格"),
                                                       QString(), filter);
        if (!paths.isEmpty()) emit boundaryImportRequested(paths);
        return;
    }
    if (meshes_.size() >= 10) return;
    MeshUiState state;
    state.name = QStringLiteral("Mesh_%1").arg(meshes_.size() + 1, 3, 10, QLatin1Char('0'));
    meshes_.append(state);
    const int row = meshTable_->rowCount();
    meshTable_->insertRow(row);
    {
        // Building a checkable cell emits itemChanged before its initial check
        // state is assigned. Do not interpret that as a user hiding the mesh.
        const QSignalBlocker block(meshTable_);
        auto *indexItem = new QTableWidgetItem;
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        auto *nameItem = new QTableWidgetItem;
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        auto *visibleItem = new QTableWidgetItem;
        visibleItem->setFlags(visibleItem->flags() | Qt::ItemIsUserCheckable);
        visibleItem->setTextAlignment(Qt::AlignCenter);
        meshTable_->setItem(row, 0, indexItem);
        meshTable_->setItem(row, 1, nameItem);
        meshTable_->setItem(row, 2, visibleItem);

    }
    refreshTableRow(row);
    meshTable_->setCurrentCell(row, 1);
    appendHistory(uiText(u"\u6dfb\u52a0\u7f51\u683c %1\uff08Mock\uff09").arg(state.name));
    updateCount();
}

void MeshManagementPage::removeCurrentMesh()
{
    const int row = selectedRow();
    if (row < 0 || row >= meshes_.size()) return;
    const QString name = meshes_[row].name;
    if (boundaryMode_) emit boundaryRemoveRequested(meshes_[row].id);
    {
        const QSignalBlocker block(meshTable_);
        meshes_.remove(row);
        meshTable_->removeRow(row);
        for (int index = 0; index < meshes_.size(); ++index) refreshTableRow(index);
        if (!meshes_.isEmpty()) meshTable_->setCurrentCell(qMin(row, meshes_.size() - 1), 1);
    }
    refreshSelection();
    appendHistory(uiText(u"\u79fb\u9664\u7f51\u683c %1\uff08\u4ec5\u5f71\u54cd\u5f53\u524d\u754c\u9762\uff09").arg(name));
    updateCount();
}

void MeshManagementPage::selectMesh(int row)
{
    refreshSelection();
    if (row < 0 || row >= meshes_.size()) return;
    appendHistory(uiText(u"\u9009\u62e9\u7f51\u683c %1").arg(meshes_[row].name));
}

void MeshManagementPage::refreshTableRow(int row)
{
    if (row < 0 || row >= meshes_.size()) return;
    auto *indexItem = meshTable_->item(row, 0);
    auto *nameItem = meshTable_->item(row, 1);
    auto *visibleItem = meshTable_->item(row, 2);
    if (!indexItem || !nameItem || !visibleItem) return;
    const QSignalBlocker block(meshTable_);
    indexItem->setText(QString::number(row + 1));
    indexItem->setTextAlignment(Qt::AlignCenter);
    nameItem->setText(meshes_[row].name + (meshes_[row].ready ? QString() : QString::fromUtf8(" · 加载中")));
    if (!meshes_[row].id.isEmpty()) nameItem->setData(Qt::UserRole, meshes_[row].id);
    visibleItem->setFlags(visibleItem->flags().setFlag(Qt::ItemIsUserCheckable, meshes_[row].ready));
    visibleItem->setCheckState(meshes_[row].visible ? Qt::Checked : Qt::Unchecked);
    visibleItem->setTextAlignment(Qt::AlignCenter);

}

void MeshManagementPage::refreshSelection()
{
    const int row = selectedRow();
    deleteMeshButton_->setEnabled(row >= 0);
    if (boundaryMode_) {
        viewerStatus_->setText(row >= 0 ? meshes_[row].summary
                                       : QString::fromUtf8("请选择网格文件以显示边界。"));
    }
}

void MeshManagementPage::updateCount()
{
    if (boundaryMode_) {
        meshCount_->setText(QString::fromUtf8("%1 个").arg(meshes_.size()));
        importMeshButton_->setEnabled(!boundaryBusy_);
        importMeshButton_->setToolTip(QString::fromUtf8("读取网格并提取全部拓扑边界面"));
        return;
    }
    meshCount_->setText(uiText(u"%1 / 10 个").arg(meshes_.size()));
    importMeshButton_->setEnabled(meshes_.size() < 10);
    importMeshButton_->setToolTip(meshes_.size() < 10
        ? uiText(u"\u6dfb\u52a0\u4e00\u4e2a\u524d\u7aef Mock \u7f51\u683c\uff0c\u6700\u591a 10 \u4e2a")
        : uiText(u"\u5df2\u8fbe\u5230 10 \u4e2a\u7f51\u683c\u5bf9\u8c61\u7684\u754c\u9762\u4e0a\u9650"));
}

void MeshManagementPage::appendHistory(const QString &message)
{
    QString displayMessage=message;
    if (boundaryMode_) {
        displayMessage.remove(QString::fromUtf8("（Mock）"));
        displayMessage.remove(QString::fromUtf8("（仅影响当前界面）"));
    }
    historyList_->addItem(QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), displayMessage));
    historyList_->scrollToBottom();
}

void MeshManagementPage::setHistoryCollapsed(bool collapsed)
{
    historyCollapsed_ = collapsed;
    auto *panel = settingsSplitter_->widget(1);
    if (collapsed) {
        const QList<int> sizes = settingsSplitter_->sizes();
        if (sizes.size() == 2 && sizes[1] > 60) expandedHistoryHeight_ = sizes[1];
        historyTabs_->hide();
        historyCollapseButton_->setText(uiText(u"\u5c55\u5f00"));
        const int headerHeight = historyHeader_->sizeHint().height() + 2;
        panel->setMaximumHeight(headerHeight);
        settingsSplitter_->setSizes({qMax(200, height()), headerHeight});
    } else {
        panel->setMaximumHeight(QWIDGETSIZE_MAX);
        historyTabs_->show();
        historyCollapseButton_->setText(uiText(u"\u6536\u8d77"));
        settingsSplitter_->setSizes({qMax(220, height() - expandedHistoryHeight_), expandedHistoryHeight_});
    }
}

QVariantMap MeshManagementPage::layoutState() const
{
    const QList<int> horizontal = mainSplitter_->sizes();
    const int horizontalTotal = horizontal.value(0) + horizontal.value(1);
    const QList<int> vertical = settingsSplitter_->sizes();
    const int verticalTotal = vertical.value(0) + vertical.value(1);
    QVariantMap state;
    state.insert(QStringLiteral("objectPanelRatio"), horizontalTotal > 0
        ? double(horizontal.value(0)) / horizontalTotal : 0.67);
    state.insert(QStringLiteral("historyRatio"), verticalTotal > 0 && !historyCollapsed_
        ? double(vertical.value(1)) / verticalTotal
        : double(expandedHistoryHeight_) / qMax(1, height()));
    state.insert(QStringLiteral("historyCollapsed"), historyCollapsed_);
    state.insert(QStringLiteral("meshToolbarArea"), int(viewportWindow_->toolBarArea(viewToolbar_)));
    return state;
}

void MeshManagementPage::restoreLayoutState(const QVariantMap &state)
{
    const double objectRatio = qBound(0.18, state.value(QStringLiteral("objectPanelRatio"), 0.67).toDouble(), 0.78);
    const double historyRatio = qBound(0.10, state.value(QStringLiteral("historyRatio"), 0.24).toDouble(), 0.55);
    mainSplitter_->setSizes({qRound(objectRatio * 1000), qRound((1.0 - objectRatio) * 1000)});
    expandedHistoryHeight_ = qBound(90, qRound(qMax(450, height()) * historyRatio), qMax(120, height() / 2));
    setHistoryCollapsed(state.value(QStringLiteral("historyCollapsed"), true).toBool());
    auto toolbarArea = static_cast<Qt::ToolBarArea>(state.value(QStringLiteral("meshToolbarArea")).toInt());
    if (toolbarArea != Qt::LeftToolBarArea && toolbarArea != Qt::RightToolBarArea
        && toolbarArea != Qt::TopToolBarArea && toolbarArea != Qt::BottomToolBarArea)
        toolbarArea = Qt::RightToolBarArea;
    viewportWindow_->addToolBar(toolbarArea, viewToolbar_);
}

void MeshManagementPage::restoreDefaultLayout()
{
    mainSplitter_->setSizes({670, 330});
    expandedHistoryHeight_ = 150;
    setHistoryCollapsed(true);
    viewportWindow_->addToolBar(Qt::RightToolBarArea, viewToolbar_);
}

void MeshManagementPage::enableBoundaryMode(bool cgnsAvailable)
{
    if (boundaryMode_) return;
    boundaryMode_ = true;
    cgnsAvailable_ = cgnsAvailable;
    static_cast<AxisTriad *>(axisIndicator_)->enableInteraction();
    findChild<QLabel *>(QStringLiteral("meshScopeLabel"))->setText(QString::fromUtf8("三维网格预览"));
    {
        QSignalBlocker block(meshTable_);
        meshTable_->setRowCount(0);
        meshes_.clear();
    }
    historyList_->clear();
    for (auto *action : viewToolbar_->actions()) {
        if (action->property("boundaryCommand").isValid()) action->setEnabled(true);
    }
    const auto *canvas = findChild<QFrame *>(QStringLiteral("meshViewerCanvas"));
    emit boundaryBackgroundRequested(canvas->property("backgroundMode").toInt());
    viewerStatus_->setWordWrap(true);
    viewerStatus_->show();
    refreshSelection(); updateCount();
    viewerStatus_->setText(QString::fromUtf8("网格按文件坐标定位。拖动 XYZ 方向球可旋转视角。"));
}
void MeshManagementPage::setBoundaryCameraOrientation(const QVector3D &from, const QVector3D &up)
{
    static_cast<AxisTriad *>(axisIndicator_)->setView(from, up);
}

void MeshManagementPage::selectBoundary(const QString &id)
{
    if (!boundaryMode_) return;
    if (id.isEmpty()) {
        meshTable_->selectionModel()->clear();
        return;
    }
    for (int row = 0; row < meshes_.size(); ++row) {
        if (meshes_[row].id != id) continue;
        meshTable_->setCurrentCell(row, 1);
        meshTable_->scrollToItem(meshTable_->item(row, 1));
        return;
    }
}

void MeshManagementPage::beginBoundary(const QString &id,const QString &path)
{
    MeshUiState state; state.id=id; state.ready=false; state.name=QFileInfo(path).fileName();
    state.summary=QString::fromUtf8("正在后台读取并提取边界…");
    meshes_.append(state);
    const int row = meshTable_->rowCount();
    meshTable_->insertRow(row);
    {
        // Building a checkable cell emits itemChanged before its initial check
        // state is assigned. Do not interpret that as a user hiding the mesh.
        const QSignalBlocker block(meshTable_);
        auto *indexItem = new QTableWidgetItem;
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        auto *nameItem = new QTableWidgetItem;
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        auto *visibleItem = new QTableWidgetItem;
        visibleItem->setFlags(visibleItem->flags() | Qt::ItemIsUserCheckable);
        visibleItem->setTextAlignment(Qt::AlignCenter);
        meshTable_->setItem(row, 0, indexItem);
        meshTable_->setItem(row, 1, nameItem);
        meshTable_->setItem(row, 2, visibleItem);

    }
    refreshTableRow(row);
    meshTable_->setCurrentCell(row, 1); refreshSelection(); updateCount();
}
void MeshManagementPage::finishBoundary(const QString &id, const QString &summary, qint64 boundaryFaces)
{
    constexpr qint64 largeSurfaceFaces = 10000000;
    for (int row = 0; row < meshes_.size(); ++row) {
        auto &mesh = meshes_[row];
        if (mesh.id != id) continue;
        if (!mesh.ready && !displayModeChosen_ && boundaryFaces >= largeSurfaceFaces && !surfaceAction_->isChecked()) {
            surfaceAction_->setChecked(true);
            emit boundaryDisplayModeChanged(0);
        }
        mesh.ready = true;
        mesh.summary = summary;
        refreshTableRow(row);
        refreshSelection();
        appendHistory(summary);
        return;
    }
}

void MeshManagementPage::failBoundary(const QString &id,const QString &message)
{
    for(int i=0;i<meshes_.size();++i) if(meshes_[i].id==id) {
        meshes_.remove(i); meshTable_->removeRow(i); break;
    }
    for(int i=0;i<meshes_.size();++i) refreshTableRow(i);
    refreshSelection(); updateCount(); viewerStatus_->setText(message); appendHistory(message);
}
void MeshManagementPage::setBoundaryBusy(bool busy) {boundaryBusy_=busy;updateCount();}
int MeshManagementPage::selectedRow() const
{
    if (!meshTable_) return -1;
    const int row = meshTable_->currentRow();
    return row >= 0 && row < meshes_.size() ? row : -1;
}

} // namespace aerosim
