#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <SARibbonBar.h>
#include <QListWidget>
#include <QSlider>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTabWidget>

namespace {
template <size_t N>
QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}
}

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    aerosim::PostProcessWorkbenchWindow window;
    if (!window.findChild<QWidget *>(QStringLiteral("postProcessDataCategory")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessDisplayCategory")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessFeatureCategory")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessAnalysisCategory")) ||
        !window.findChild<QWidget *>(QStringLiteral("postProcessToolsCategory")))
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
    if (!pan->isChecked() || !interactionHint->text().contains(uiText(u"平移")))
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

    // Exercise the actual window after SARibbon's deferred theme/layout work.
    // Optional captures are produced by the same binary used in CTest.
    const QString captureDir = qEnvironmentVariable("AEROSIM_UI_CAPTURE_DIR");
    if (!captureDir.isEmpty()) QDir().mkpath(captureDir);
    auto *navigator = window.findChild<QTabWidget *>(QStringLiteral("postProcessNavigator"));
    if (!QFile::exists(QStringLiteral(":/SARibbonTheme/resource/palettes/office2013-default.json"))) {
        qCritical() << "SARibbon theme resources not linked";
        return 25;
    }
    window.show();
    for (const QSize size : {QSize(1120,680), QSize(1440,960)}) {
        window.resize(size);
        app.processEvents();
        for (int category = 0; category < 5; ++category) {
            window.ribbonBar()->setCurrentIndex(category);
            navigator->setCurrentIndex(category % 3);
            if (category == 0) tree->setCurrentItem(tree->topLevelItem(0)->child(0));
            if (category == 1) tree->setCurrentItem(tree->topLevelItem(1)->child(0));
            if (category == 2) layers->setCurrentRow(1);
            if (category == 3) probe->trigger();
            if (category == 4) inspectorTabs->setCurrentIndex(2);
            app.processEvents();
            if (window.size() != size) {
                qCritical() << "Window minimum grew beyond requested size" << size << window.size();
                return 20;
            }
            for (auto *button : window.findChildren<QToolButton *>()) {
                if (!button->isVisible() || !button->objectName().startsWith("postProcess")) continue;
                if (button->width() < button->iconSize().width() + 8 ||
                    button->height() < button->iconSize().height() + 8) {
                    qCritical() << "Clipped timeline button" << button->objectName() << button->size();
                    return 21;
                }
            }
            for (auto *combo : window.findChildren<QComboBox *>()) {
                if (!combo->isVisible()) continue;
                if (!combo->parentWidget()->rect().contains(combo->geometry())) {
                    qCritical() << "Inspector field outside parent" << combo->geometry();
                    return 22;
                }
            }
            if (!captureDir.isEmpty() && !window.grab().save(captureDir +
                QStringLiteral("/postprocess-%1x%2-%3.png").arg(size.width()).arg(size.height()).arg(category)))
                return 23;
        }
    }
    navigator->setCurrentIndex(1);
    window.resize(1120,680);
    for (int group : {2,3}) {
        tree->setCurrentItem(tree->topLevelItem(group)->child(0));
        app.processEvents();
        if (!captureDir.isEmpty() && !window.grab().save(captureDir +
            QStringLiteral("/inspector-%1.png").arg(group))) return 26;
    }
    auto *harness = window.findChild<QAction *>(QStringLiteral("postProcessHarnessAction"));
    harness->trigger();
    app.processEvents();
    for (auto *caption : window.findChildren<QLabel *>(QStringLiteral("postProcessDockCaption"))) {
        if (caption->isVisible() && (caption->height() < caption->sizeHint().height() ||
            caption->width() < caption->sizeHint().width())) {
            qCritical() << "Dock caption clipped" << caption->size() << caption->sizeHint();
            return 28;
        }
    }
    if (!captureDir.isEmpty() && !window.grab().save(captureDir + QStringLiteral("/harness.png"))) return 27;

    for (auto *action : window.findChildren<QAction *>()) {
        if (action->objectName().startsWith("postProcess") && action->icon().isNull()) {
            qCritical() << "Missing command icon" << action->objectName();
            return 24;
        }
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
