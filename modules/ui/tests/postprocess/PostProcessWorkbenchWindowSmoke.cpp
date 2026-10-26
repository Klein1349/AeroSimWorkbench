#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    aerosim::PostProcessWorkbenchWindow window;
    if (!window.findChild<QWidget *>(QStringLiteral("postProcessMockEntry")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessSessionOutline")) ||
        !window.findChild<QWidget *>(QStringLiteral("sceneLayerList")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessTimeSlider")))
        return 1;

    auto *entry = window.findChild<QComboBox *>(QStringLiteral("postProcessMockEntry"));
    auto *layers = window.findChild<QListWidget *>(QStringLiteral("sceneLayerList"));
    auto *contour = window.findChild<QToolButton *>(QStringLiteral("postProcessContourButton"));
    if (!entry || entry->count() != 9 || entry->currentIndex() != 1 || !layers || !contour)
        return 4;
    const int layerCount = layers->count();
    contour->click();
    if (layers->count() != layerCount + 1)
        return 7;
    entry->setCurrentIndex(6);
    if (window.findChild<QWidget *>(QStringLiteral("postProcessTimeline"))->isHidden())
        return 5;
    entry->setCurrentIndex(8);
    if (!window.findChild<QWidget *>(QStringLiteral("postProcessMeshButton"))->isEnabled()) {
        // Curve mode deliberately disables spatial-only controls.
    } else {
        return 6;
    }

    auto *viewer = new QLabel(QStringLiteral("Viewer"));
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport, viewer))
        return 2;
    auto *secondViewer = new QLabel(QStringLiteral("Second viewer"));
    if (window.insertWidget(aerosim::WidgetSlot::Viewport, secondViewer))
        return 3;
    delete secondViewer;
    return 0;
}
