#include "aerosim/visualization/scene/VtkSceneViewer.h"

#include <QApplication>
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

#include <cmath>
#include <iostream>

using aerosim::visualization::BoundaryMesh;
using aerosim::visualization::BoundarySurfaceExtractor;
using aerosim::visualization::VtkSceneViewer;

static void scroll(QWidget *viewport, const QPoint &angle, const QPoint &pixels = {})
{
    const QPoint pos = viewport->rect().center();
    QWheelEvent wheel(pos, viewport->mapToGlobal(pos), pixels, angle, Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(viewport, &wheel);
}

static bool closeTo(double actual, double expected)
{
    return std::abs(actual - expected) < 1e-8 * expected;
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

int main(int argc, char **argv)
{
    VtkSceneViewer::configureSurface();
    QApplication app(argc, argv);
    VtkSceneViewer viewer;
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
    viewer.addBoundary("first", first);
    auto *actors = renderer->GetActors();
    auto *a = actors->GetLastActor();
    if (!a || !a->GetProperty()->GetEdgeVisibility()) return 1;
    viewer.setDisplayMode(1);
    viewer.addBoundary("second", second);
    auto *b = renderer->GetActors()->GetLastActor();
    if (!b || a == b || a->GetProperty()->GetRepresentation() != VTK_WIREFRAME
        || b->GetProperty()->GetRepresentation() != VTK_WIREFRAME) {
        std::cerr << "Both current and newly imported meshes must share the view mode\n";
        return 1;
    }
    viewer.setObjectVisible("first", false);
    if (a->GetVisibility() || !b->GetVisibility()) return 1;
    viewer.setDisplayMode(2);
    viewer.setObjectVisible("first", true);
    for (auto *actor : {a, b}) {
        if (!actor->GetVisibility() || actor->GetProperty()->GetRepresentation() != VTK_SURFACE
            || !actor->GetProperty()->GetEdgeVisibility()) return 1;
    }
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
    return 0;
}
