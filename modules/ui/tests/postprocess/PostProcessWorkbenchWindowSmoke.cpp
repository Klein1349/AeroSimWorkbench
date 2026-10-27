#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QSlider>
#include <QStackedWidget>
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
    auto *probe = window.findChild<QToolButton *>(QStringLiteral("postProcessProbeButton"));
    auto *inspector = window.findChild<QStackedWidget *>(QStringLiteral("postProcessProperties"));
    auto *timeSlider = window.findChild<QSlider *>(QStringLiteral("postProcessTimeSlider"));
    if (!tree || tree->topLevelItemCount() < 4 || !layers || !contour || !probe || !inspector || !timeSlider)
        return 4;
    const int layerCount = layers->count();
    contour->click();
    if (layers->count() != layerCount + 1)
        return 7;
    if (window.findChild<QWidget *>(QStringLiteral("postProcessTimeline"))->isHidden())
        return 5;

    // Every category has its own inspector.  This protects the UI contract
    // against reverting to one generic "object properties" form.
    tree->setCurrentItem(tree->topLevelItem(1)->child(0));
    if (inspector->currentIndex() != 2)
        return 8;
    tree->setCurrentItem(tree->topLevelItem(2)->child(2));
    if (inspector->currentIndex() != 3 || timeSlider->value() != 2)
        return 9;
    tree->setCurrentItem(tree->topLevelItem(3)->child(0));
    if (inspector->currentIndex() != 4)
        return 10;
    layers->setCurrentRow(0);
    if (inspector->currentIndex() != 5)
        return 11;
    probe->click();
    if (inspector->currentIndex() != 6)
        return 12;

    auto *viewer = new QLabel(QStringLiteral("Viewer"));
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport, viewer))
        return 2;
    auto *secondViewer = new QLabel(QStringLiteral("Second viewer"));
    if (window.insertWidget(aerosim::WidgetSlot::Viewport, secondViewer))
        return 3;
    delete secondViewer;
    return 0;
}
