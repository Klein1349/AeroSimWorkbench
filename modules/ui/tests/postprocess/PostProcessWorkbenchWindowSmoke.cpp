#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QTreeWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    aerosim::PostProcessWorkbenchWindow window;
    if (!window.findChild<QWidget *>(QStringLiteral("postProcessDatasetTree")) ||
        !window.findChild<QWidget *>(QStringLiteral("sceneLayerList")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessTimeSlider")))
        return 1;

    auto *tree = window.findChild<QTreeWidget *>(QStringLiteral("postProcessDatasetTree"));
    auto *layers = window.findChild<QListWidget *>(QStringLiteral("sceneLayerList"));
    auto *contour = window.findChild<QToolButton *>(QStringLiteral("postProcessContourButton"));
    if (!tree || tree->topLevelItemCount() < 4 || !layers || !contour)
        return 4;
    const int layerCount = layers->count();
    contour->click();
    if (layers->count() != layerCount + 1)
        return 7;
    if (window.findChild<QWidget *>(QStringLiteral("postProcessTimeline"))->isHidden())
        return 5;

    auto *viewer = new QLabel(QStringLiteral("Viewer"));
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport, viewer))
        return 2;
    auto *secondViewer = new QLabel(QStringLiteral("Second viewer"));
    if (window.insertWidget(aerosim::WidgetSlot::Viewport, secondViewer))
        return 3;
    delete secondViewer;
    return 0;
}
