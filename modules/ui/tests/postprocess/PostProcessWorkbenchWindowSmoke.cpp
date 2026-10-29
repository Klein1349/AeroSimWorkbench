#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QSlider>
#include <QStackedWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QTabWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    aerosim::PostProcessWorkbenchWindow window;
    auto *menuBar = window.findChild<QMenuBar *>(QStringLiteral("postProcessMenuBar"));
    auto *quickTools = window.findChild<QToolBar *>(QStringLiteral("postProcessQuickTools"));
    if (!menuBar || menuBar->actions().size() != 5 || !quickTools || quickTools->actions().size() < 10)
        return 12;
    if (!window.findChild<QWidget *>(QStringLiteral("postProcessDatasetTree")) ||
        !window.findChild<QWidget *>(QStringLiteral("sceneLayerList")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessTimeSlider")))
        return 1;

    auto *tree = window.findChild<QTreeWidget *>(QStringLiteral("postProcessDatasetTree"));
    auto *layers = window.findChild<QListWidget *>(QStringLiteral("sceneLayerList"));
    auto *contour = window.findChild<QAction *>(QStringLiteral("postProcessContourAction"));
    auto *probe = window.findChild<QAction *>(QStringLiteral("postProcessProbeAction"));
    auto *pan = window.findChild<QAction *>(QStringLiteral("postProcessPanAction"));
    auto *topView = window.findChild<QAction *>(QStringLiteral("postProcessTopViewAction"));
    auto *inspector = window.findChild<QStackedWidget *>(QStringLiteral("postProcessProperties"));
    auto *inspectorTabs = window.findChild<QTabWidget *>(QStringLiteral("postProcessInspectorTabs"));
    auto *timeSlider = window.findChild<QSlider *>(QStringLiteral("postProcessTimeSlider"));
    auto *interactionHint = window.findChild<QLabel *>(QStringLiteral("postProcessInteractionHint"));
    if (!tree || tree->topLevelItemCount() < 4 || !layers || !contour || !probe || !pan || !topView || !interactionHint || !inspector || !inspectorTabs || inspectorTabs->count() != 3 || !timeSlider)
        return 4;
    pan->trigger();
    if (!pan->isChecked() || !interactionHint->text().contains(QString::fromUtf16(reinterpret_cast<const ushort *>(u"\u5e73\u79fb"))))
        return 13;
    const int layerCount = layers->count();
    contour->trigger();
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
    probe->trigger();
    if (inspectorTabs->currentIndex() != 1)
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
