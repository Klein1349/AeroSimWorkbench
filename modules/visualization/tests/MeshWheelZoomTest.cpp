#include "aerosim/visualization/scene/MeshSceneViewer.h"

#include <QApplication>
#include <QKeyEvent>
#include <QVTKOpenGLNativeWidget.h>
#include <QWheelEvent>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>

#include <algorithm>
#include <cmath>
#include <iostream>

using aerosim::visualization::BoundaryMesh;
using aerosim::visualization::BoundarySurfaceExtractor;
using aerosim::visualization::MeshSceneViewer;

static void scroll(QWidget *viewport, const QPoint &angle, const QPoint &pixels = {})
{
    const QPoint pos = viewport->rect().center();
    QWheelEvent wheel(pos, viewport->mapToGlobal(pos), pixels, angle, Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(viewport, &wheel);
}

static bool closeTo(double actual, double expected)
{
    return std::abs(actual - expected) <= 1e-8 * std::max(1.0, std::abs(expected));
}

static bool atFocus(vtkCamera *camera, double x, double y, double z)
{
    const auto *focus = camera->GetFocalPoint();
    return closeTo(focus[0], x) && closeTo(focus[1], y) && closeTo(focus[2], z);
}

// Use separated triangles with nonzero coordinates on every axis. Centering
// either mesh individually would break their relative placement.
static std::shared_ptr<const BoundaryMesh> triangleAt(double x)
{
    vtkNew<vtkPoints> points;
    points->InsertNextPoint(x, -4, 12);
    points->InsertNextPoint(x + 3, -4, 12);
    points->InsertNextPoint(x, 2, 15);
    vtkNew<vtkCellArray> polys;
    const vtkIdType triangle[] = {0, 1, 2};
    polys->InsertNextCell(3, triangle);
    vtkNew<vtkPolyData> input;
    input->SetPoints(points);
    input->SetPolys(polys);
    return BoundarySurfaceExtractor::extract(input);
}

static bool atFileCoordinates(vtkActor *actor, const BoundaryMesh &mesh, double x)
{
    auto *mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
    if (!mapper || mapper->GetInput() != mesh.surface.GetPointer() || actor->GetDragable())
        return false;
    auto *matrix = actor->GetMatrix();
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            if (matrix->GetElement(row, col) != (row == col ? 1.0 : 0.0)) return false;

    const double expected[3][3] = {{x, -4, 12}, {x + 3, -4, 12}, {x, 2, 15}};
    for (vtkIdType i = 0; i < 3; ++i) {
        double point[3];
        mapper->GetInput()->GetPoint(i, point);
        for (int axis = 0; axis < 3; ++axis)
            if (point[axis] != expected[i][axis]) return false;
    }
    return true;
}

static bool hasAppearance(vtkActor *actor, const QColor &color, double opacity,
                          int representation, bool edges)
{
    auto *property = actor->GetProperty();
    const auto *rgb = property->GetDiffuseColor();
    return std::abs(rgb[0] - color.redF()) < 1e-6
        && std::abs(rgb[1] - color.greenF()) < 1e-6
        && std::abs(rgb[2] - color.blueF()) < 1e-6
        && std::abs(property->GetOpacity() - opacity) < 1e-6
        && property->GetRepresentation() == representation
        && bool(property->GetEdgeVisibility()) == edges;
}

int main(int argc, char **argv)
{
    MeshSceneViewer::configureSurface();
    QApplication app(argc, argv);
    MeshSceneViewer viewer;
    auto *viewport = viewer.findChild<QVTKOpenGLNativeWidget *>();
    if (!viewport) return 1;
    auto *renderer = viewport->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    auto *camera = renderer->GetActiveCamera();

    const double distance = camera->GetDistance();
    scroll(viewport, QPoint(0, 60));
    scroll(viewport, QPoint(0, 60));
    if (!closeTo(camera->GetDistance(), distance / 1.2)) {
        std::cerr << "Two half-notches should zoom in by exactly one step\n";
        return 1;
    }
    scroll(viewport, QPoint(0, -120));
    if (!closeTo(camera->GetDistance(), distance)) {
        std::cerr << "Reverse wheel should restore perspective camera distance\n";
        return 1;
    }

    camera->ParallelProjectionOn();
    const double scale = camera->GetParallelScale();
    scroll(viewport, QPoint(), QPoint(0, 120));
    scroll(viewport, QPoint(120, 0));
    if (!closeTo(camera->GetParallelScale(), scale / 1.2)) {
        std::cerr << "Only vertical wheel input should change parallel scale\n";
        return 1;
    }

    auto first = triangleAt(125);
    auto second = triangleAt(-470);
    const QColor initialColor(0xbc, 0xa9, 0xe4);
    const QColor firstColor(0x18, 0x82, 0xc5);
    const QColor secondColor(0xd5, 0x41, 0x23);
    viewer.addBoundary("first", first, 2);
    auto *a = renderer->GetActors()->GetLastActor();
    if (!a || !hasAppearance(a, initialColor, 1.0, VTK_SURFACE, true)) return 1;
    viewer.setObjectAppearance("first", firstColor, 35, 1);
    // Mode supplied by the page for a large boundary, before the first render.
    viewer.addBoundary("second", second, 0);
    auto *b = renderer->GetActors()->GetLastActor();
    if (!b || a == b || !hasAppearance(a, firstColor, 0.35, VTK_WIREFRAME, false)
        || !hasAppearance(b, initialColor, 1.0, VTK_SURFACE, false)) {
        std::cerr << "A new surface-only mesh must start with its own appearance\n";
        return 1;
    }
    vtkNew<vtkActor> auxiliary;
    auxiliary->SetMapper(b->GetMapper());
    auxiliary->SetPosition(25000, 25000, 25000);
    renderer->AddActor(auxiliary);
    viewer.setCameraOrientation(QStringLiteral("fit"));
    if (!atFocus(camera, -171, -1, 13.5)) {
        std::cerr << "Fit should enclose only visible boundary meshes\n";
        return 1;
    }
    viewer.orbitCamera(13, -7);
    const double visibleScale = camera->GetParallelScale();
    const double visibleDistance = camera->GetDistance();
    double direction[3];
    camera->GetDirectionOfProjection(direction);
    viewer.setObjectVisible("first", false);
    const auto *newDirection = camera->GetDirectionOfProjection();
    if (!atFocus(camera, -468.5, -1, 13.5) || !closeTo(camera->GetParallelScale(), visibleScale)
        || !closeTo(camera->GetDistance(), visibleDistance)
        || !closeTo(direction[0], newDirection[0]) || !closeTo(direction[1], newDirection[1])
        || !closeTo(direction[2], newDirection[2])) {
        std::cerr << "Hiding a mesh should recenter on the visible data and preserve the camera view\n";
        return 1;
    }
    // Simulate a panned view, then invoke the explicit rotation-center command.
    camera->SetFocalPoint(-460, 5, 10);
    viewer.setCameraOrientation(QStringLiteral("center"));
    auto *marker = viewport->findChild<QWidget *>(QStringLiteral("boundaryRotationCenterMarker"));
    if (!atFocus(camera, -468.5, -1, 13.5) || !marker || marker->isHidden()
        || !marker->testAttribute(Qt::WA_TransparentForMouseEvents)
        || (marker->geometry().center() - viewport->rect().center()).manhattanLength() > 1) return 1;
    viewer.setObjectVisible("second", false);
    const double emptyScale = camera->GetParallelScale();
    const double emptyDistance = camera->GetDistance();
    viewer.setCameraOrientation(QStringLiteral("fit"));
    viewer.setCameraOrientation(QStringLiteral("center"));
    if (!marker->isHidden() || !atFocus(camera, -468.5, -1, 13.5)
        || !closeTo(camera->GetParallelScale(), emptyScale)
        || !closeTo(camera->GetDistance(), emptyDistance)) {
        std::cerr << "An empty boundary selection must retain a valid camera\n";
        return 1;
    }
    viewer.setObjectVisible("second", true);
    viewer.setObjectAppearance("second", secondColor, 70, 2);
    viewer.setObjectAppearance("missing", initialColor, 100, 0);
    QKeyEvent wireframe(QEvent::KeyPress, Qt::Key_W, Qt::NoModifier, "w");
    QApplication::sendEvent(viewport, &wireframe);
    if (a->GetVisibility() || !b->GetVisibility()
        || !hasAppearance(a, firstColor, 0.35, VTK_WIREFRAME, false)
        || !hasAppearance(b, secondColor, 0.70, VTK_SURFACE, true)) {
        std::cerr << "Appearance and visibility changes must affect only the named mesh\n";
        return 1;
    }
    viewer.setObjectVisible("first", true);
    if (!a->GetVisibility() || !hasAppearance(a, firstColor, 0.35, VTK_WIREFRAME, false)
        || !atFocus(camera, -171, -1, 13.5))
        return 1;
    viewer.setCameraOrientation(QStringLiteral("fit"));
    double previous[3];
    camera->GetPosition(previous);
    viewer.orbitCamera(30, 18);
    const auto *position = camera->GetPosition();
    if (position[0] == previous[0] && position[1] == previous[1] && position[2] == previous[2]) {
        std::cerr << "Orbit must move the camera\n";
        return 1;
    }
    const double fittedScale = camera->GetParallelScale();
    scroll(viewport, QPoint(0, 120));
    if (!closeTo(camera->GetParallelScale(), fittedScale / 1.2)
        || !atFileCoordinates(a, *first, 125) || !atFileCoordinates(b, *second, -470)) {
        std::cerr << "View controls must retain the original coordinates of both meshes\n";
        return 1;
    }
    viewer.removeObject("second");
    if (!atFocus(camera, 126.5, -1, 13.5)) return 1;
    return 0;
}
