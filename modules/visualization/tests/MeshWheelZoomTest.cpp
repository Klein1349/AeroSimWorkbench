#include "aerosim/visualization/scene/VtkSceneViewer.h"

#include <QApplication>
#include <QVTKOpenGLNativeWidget.h>
#include <QWheelEvent>
#include <vtkCamera.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>

#include <cmath>
#include <iostream>

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
    return 0;
}
