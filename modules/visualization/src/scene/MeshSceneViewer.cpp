#include "aerosim/visualization/scene/MeshSceneViewer.h"
#include "aerosim/visualization/scene/BoundaryPolyDataMapper.h"
#include <QVTKOpenGLNativeWidget.h>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QThread>
#include <QSurfaceFormat>
#include <QHash>
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
#include <vtkActor.h>
#include <vtkBoundingBox.h>
#include <vtkCamera.h>
#include <vtkCallbackCommand.h>
#include <vtkCellPicker.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <array>
#include <cmath>
namespace aerosim { namespace visualization {
namespace {
class RotationCenterMarker final : public QWidget
{
public:
    explicit RotationCenterMarker(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("boundaryRotationCenterMarker"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedSize(31, 31);
        timer_.setSingleShot(true);
        connect(&timer_, &QTimer::timeout, this, &QWidget::hide);
        hide();
    }

    void indicateCenter()
    {
        move((parentWidget()->width() - width()) / 2, (parentWidget()->height() - height()) / 2);
        raise();
        show();
        timer_.start(1500);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(15.5, 15.5);
        const QLine ticks[] = {{-13, 0, -5, 0}, {5, 0, 13, 0}, {0, -13, 0, -5}, {0, 5, 0, 13}};
        for (const QPen &pen : {QPen(Qt::black, 3), QPen(Qt::white, 1)}) {
            painter.setPen(pen);
            painter.drawEllipse(QPointF(0, 0), 5, 5);
            painter.drawLines(ticks, 4);
            painter.drawPoint(QPointF(0, 0));
        }
    }

private:
    QTimer timer_;
};

void zoomCamera(vtkCamera *camera, double steps)
{
    const double factor = std::pow(1.2, steps);
    if (camera->GetParallelProjection())
        camera->SetParallelScale(camera->GetParallelScale() / factor);
    else
        camera->Dolly(factor);
}

void applyDisplayMode(vtkProperty *property, int mode)
{
    property->SetRepresentation(mode == 1 ? VTK_WIREFRAME : VTK_SURFACE);
    property->SetEdgeVisibility(mode == 2);
}
}
struct MeshSceneViewer::Impl {
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> window=vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    vtkSmartPointer<vtkRenderer> renderer=vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
        vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    unsigned long interactionObserver = 0;
    QHash<QString, vtkSmartPointer<vtkActor>> objects;
    vtkBoundingBox visibleBounds;
    QVTKOpenGLNativeWidget *viewport = nullptr;
    RotationCenterMarker *centerMarker = nullptr;
    QPoint pickPressPosition;
    bool pickPending = false;

    void updateVisibleBounds()
    {
        visibleBounds.Reset();
        for (const auto &actor : objects) {
            if (!actor->GetVisibility()) continue;
            if (const auto *bounds = actor->GetBounds()) visibleBounds.AddBounds(bounds);
        }
    }

    void centerVisibleMeshes()
    {
        if (!visibleBounds.IsValid()) return;
        auto *camera = renderer->GetActiveCamera();
        double center[3], focus[3], position[3];
        visibleBounds.GetCenter(center);
        camera->GetFocalPoint(focus);
        camera->GetPosition(position);
        for (int axis = 0; axis < 3; ++axis)
            position[axis] += center[axis] - focus[axis];
        camera->SetPosition(position);
        camera->SetFocalPoint(center);
    }

    void updateClippingRange()
    {
        if (!visibleBounds.IsValid()) return;
        double bounds[6];
        visibleBounds.GetBounds(bounds);
        renderer->ResetCameraClippingRange(bounds);
    }
};
MeshSceneViewer::MeshSceneViewer(QWidget* parent):QWidget(parent),impl_(new Impl) {
    setObjectName(QStringLiteral("boundaryVtkViewer"));
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
    auto* widget=new QVTKOpenGLNativeWidget(this); layout->addWidget(widget);
    impl_->window->AddRenderer(impl_->renderer); widget->SetRenderWindow(impl_->window);
    impl_->viewport = widget;
    impl_->centerMarker = new RotationCenterMarker(widget);
    widget->installEventFilter(this);
    impl_->window->GetInteractor()->SetInteractorStyle(impl_->style);
    vtkNew<vtkCallbackCommand> interaction;
    interaction->SetClientData(this);
    interaction->SetCallback([](vtkObject *, unsigned long, void *client, void *) {
        static_cast<MeshSceneViewer *>(client)->publishCameraOrientation();
    });
    impl_->interactionObserver = impl_->style->AddObserver(vtkCommand::InteractionEvent, interaction);
    impl_->renderer->SetBackground(0.15,0.19,0.25);
    impl_->renderer->SetBackground2(0.85,0.9,0.96); impl_->renderer->GradientBackgroundOn();
}
MeshSceneViewer::~MeshSceneViewer()
{
    impl_->viewport->removeEventFilter(this);
    impl_->style->RemoveObserver(impl_->interactionObserver);
}

void MeshSceneViewer::configureSurface()
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
}
void MeshSceneViewer::render() {
    Q_ASSERT(QThread::currentThread()==thread());
    if(isVisible()) impl_->window->Render();
}
void MeshSceneViewer::addBoundary(const QString &id, std::shared_ptr<const BoundaryMesh> mesh, int initialMode) {
    if(!mesh || impl_->objects.contains(id)) return;
    vtkNew<BoundaryPolyDataMapper> mapper;
    mapper->SetInputData(mesh->surface);
    mapper->SetSharedEdges(mesh->sharedEdges);
    mapper->ScalarVisibilityOff();
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    // File coordinates are world coordinates for every mesh in the scene.
    actor->DragableOff();
    auto *property = actor->GetProperty();
    const QColor surfaceColor(0xbc, 0xa9, 0xe4);
    property->SetColor(surfaceColor.redF(), surfaceColor.greenF(), surfaceColor.blueF());
    property->SetEdgeColor(0.1, 0.15, 0.2);
    applyDisplayMode(property, initialMode);
    impl_->renderer->AddActor(actor);
    impl_->objects.insert(id, actor);
    refreshVisibleBounds(impl_->objects.size() == 1);
}
void MeshSceneViewer::removeObject(const QString& id) {
    if(!impl_->objects.contains(id)) return;
    const auto actor = impl_->objects.take(id);
    impl_->renderer->RemoveActor(actor);
    if (actor->GetVisibility()) refreshVisibleBounds(false);
    else render();
}
void MeshSceneViewer::setObjectVisible(const QString &id, bool visible)
{
    auto actor = impl_->objects.value(id);
    if (!actor || bool(actor->GetVisibility()) == visible) return;
    actor->SetVisibility(visible);
    refreshVisibleBounds(false);
}

void MeshSceneViewer::refreshVisibleBounds(bool fit)
{
    impl_->centerMarker->hide();
    impl_->updateVisibleBounds();
    if (fit) {
        setCameraOrientation(QStringLiteral("fit"));
        return;
    }
    impl_->centerVisibleMeshes();
    impl_->updateClippingRange();
    render();
}

void MeshSceneViewer::setObjectAppearance(const QString &id, const QColor &color,
                                         int opacityPercent, int mode)
{
    auto actor = impl_->objects.value(id);
    if (!actor || !color.isValid() || mode < 0 || mode > 2) return;
    auto *property = actor->GetProperty();
    property->SetColor(color.redF(), color.greenF(), color.blueF());
    property->SetOpacity(qBound(0, opacityPercent, 100) / 100.0);
    applyDisplayMode(property, mode);
    render();
}
void MeshSceneViewer::setBackground(const QColor& b,const QColor& t,bool g) {
    impl_->renderer->SetBackground(b.redF(),b.greenF(),b.blueF());
    impl_->renderer->SetBackground2(t.redF(),t.greenF(),t.blueF()); impl_->renderer->SetGradientBackground(g); render();
}
void MeshSceneViewer::setCameraOrientation(const QString &command)
{
    auto *camera = impl_->renderer->GetActiveCamera();
    const bool reset = command == QStringLiteral("reset");
    if (command == QStringLiteral("center")) {
        impl_->centerVisibleMeshes();
        if (impl_->visibleBounds.IsValid()) impl_->centerMarker->indicateCenter();
    } else if (command == QStringLiteral("fit") || reset) {
        if (impl_->visibleBounds.IsValid()) {
            double bounds[6];
            impl_->visibleBounds.GetBounds(bounds);
            if (reset) {
                const auto *focus = camera->GetFocalPoint();
                camera->SetPosition(focus[0], focus[1], focus[2] + camera->GetDistance());
                camera->SetViewUp(0, 1, 0);
            }
            impl_->renderer->ResetCamera(bounds);
        }
    } else if (command == QStringLiteral("in") || command == QStringLiteral("out")) {
        zoomCamera(camera, command == QStringLiteral("in") ? 1.0 : -1.0);
    } else {
        struct View { const char *name; QVector3D from; QVector3D up; };
        const std::array<View, 7> views = {{
            {"front", {0, -1, 0}, {0, 0, 1}},
            {"back", {0, 1, 0}, {0, 0, 1}},
            {"right", {1, 0, 0}, {0, 0, 1}},
            {"left", {-1, 0, 0}, {0, 0, 1}},
            {"top", {0, 0, 1}, {0, 1, 0}},
            {"bottom", {0, 0, -1}, {0, 1, 0}},
            {"iso", {1, -1, 1}, {0, 0, 1}},
        }};
        for (const auto &view : views) {
            if (command != QLatin1String(view.name)) continue;
            const auto *center = camera->GetFocalPoint();
            const auto offset = view.from.normalized();
            const double distance = camera->GetDistance();
            camera->SetPosition(center[0] + distance * offset.x(),
                                center[1] + distance * offset.y(),
                                center[2] + distance * offset.z());
            camera->SetViewUp(view.up.x(), view.up.y(), view.up.z());
            camera->OrthogonalizeViewUp();
            break;
        }
    }
    impl_->updateClippingRange();
    publishCameraOrientation();
    render();
}

void MeshSceneViewer::orbitCamera(int dx, int dy)
{
    auto *camera = impl_->renderer->GetActiveCamera();
    constexpr double degreesPerPixel = 0.65;
    camera->Azimuth(-dx * degreesPerPixel);
    camera->Elevation(-dy * degreesPerPixel);
    camera->OrthogonalizeViewUp();
    impl_->updateClippingRange();
    publishCameraOrientation();
    render();
}

bool MeshSceneViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != impl_->viewport) return QWidget::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::Resize:
        impl_->centerMarker->hide();
        break;
    case QEvent::KeyPress: {
        auto *key = static_cast<QKeyEvent *>(event);
        // The page handles W/S for the selected mesh. VTK's fallback is scene-wide.
        if (key->key() == Qt::Key_W || key->key() == Qt::Key_S) {
            key->accept();
            return true;
        }
        if (key->key() == Qt::Key_R && key->modifiers() == Qt::NoModifier) {
            setCameraOrientation(QStringLiteral("fit"));
            key->accept();
            return true;
        }
        break;
    }
    case QEvent::Wheel: {
        auto *wheel = static_cast<QWheelEvent *>(event);
        // A conventional wheel notch is 120 angle units. Keep fractional
        // steps for high-resolution wheels and pixel-only touchpad events.
        const int delta = wheel->angleDelta().isNull() ? wheel->pixelDelta().y() : wheel->angleDelta().y();
        const double steps = double(delta) / QWheelEvent::DefaultDeltasPerStep;
        if (steps != 0.0) {
            zoomCamera(impl_->renderer->GetActiveCamera(), steps);
            impl_->updateClippingRange();
            render();
        }
        wheel->accept();
        return true;
    }
    case QEvent::MouseButtonPress: {
        impl_->centerMarker->hide();
        auto *mouse = static_cast<QMouseEvent *>(event);
        impl_->pickPending = mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier;
        impl_->pickPressPosition = mouse->pos();
        break;
    }
    case QEvent::MouseMove:
        if ((static_cast<QMouseEvent *>(event)->pos() - impl_->pickPressPosition).manhattanLength()
            >= QApplication::startDragDistance()) impl_->pickPending = false;
        break;
    case QEvent::MouseButtonRelease: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const bool isClick = impl_->pickPending && mouse->button() == Qt::LeftButton
            && mouse->modifiers() == Qt::NoModifier && impl_->viewport->rect().contains(mouse->pos())
            && (mouse->pos() - impl_->pickPressPosition).manhattanLength() < QApplication::startDragDistance();
        impl_->pickPending = false;
        if (isClick) pickBoundary(mouse->pos());
        break;
    }
    case QEvent::UngrabMouse:
        impl_->pickPending = false;
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void MeshSceneViewer::pickBoundary(const QPoint &position)
{
    const auto *size = impl_->window->GetSize();
    if (size[0] <= 0 || size[1] <= 0 || impl_->viewport->width() <= 0 || impl_->viewport->height() <= 0) return;
    // Qt uses logical pixels from the top-left. VTK uses framebuffer pixels from the bottom-left.
    const double x = position.x() * double(size[0]) / impl_->viewport->width();
    const double y = size[1] - 1 - position.y() * double(size[1]) / impl_->viewport->height();
    vtkNew<vtkCellPicker> picker;
    picker->Pick(x, y, 0, impl_->renderer);
    auto *pickedActor = picker->GetActor();
    for (auto item = impl_->objects.cbegin(); item != impl_->objects.cend(); ++item) {
        if (item.value().GetPointer() != pickedActor) continue;
        emit boundaryPicked(item.key());
        return;
    }
    emit boundaryPicked(QString());
}

void MeshSceneViewer::publishCameraOrientation()
{
    auto *camera = impl_->renderer->GetActiveCamera();
    const auto *direction = camera->GetDirectionOfProjection();
    const auto *up = camera->GetViewUp();
    emit cameraOrientationChanged(QVector3D(-direction[0], -direction[1], -direction[2]),
                                  QVector3D(up[0], up[1], up[2]));
}
} }
