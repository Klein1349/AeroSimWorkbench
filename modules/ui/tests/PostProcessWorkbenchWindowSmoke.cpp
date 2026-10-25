#include "aerosim/ui/PostProcessWorkbenchWindow.h"

#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    aerosim::PostProcessWorkbenchWindow window;
    if (!window.findChild<QWidget *>(QStringLiteral("postProcessDataTree")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessSceneTree")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessTimeSlider")))
        return 1;

    auto *viewer = new QLabel(QStringLiteral("Viewer"));
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport, viewer))
        return 2;
    auto *secondViewer = new QLabel(QStringLiteral("Second viewer"));
    if (window.insertWidget(aerosim::WidgetSlot::Viewport, secondViewer))
        return 3;
    delete secondViewer;
    return 0;
}
