#pragma once

#include "aerosim/ui/IntegrationPorts.h"
#include <SARibbonMainWindow.h>

#include <QList>
#include <QHash>
#include <QPointer>
#include <QVariantMap>

class QAction;
class QDialog;
class QLabel;
class QLineEdit;
class QListWidget;
class QDockWidget;
class QSlider;
class QStackedWidget;
class QTabWidget;
class QTextEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QWidget;

namespace aerosim {

// The post-processing workspace owns Qt controls only. A visualization scene
// service supplies the actual VTK widget through WidgetSlot::Viewport.
class PostProcessWorkbenchWindow final : public SARibbonMainWindow, public IWidgetHost {
    Q_OBJECT
public:
    explicit PostProcessWorkbenchWindow(QWidget *parent = nullptr);

    QWidget *windowWidget() override { return this; }
    bool insertWidget(WidgetSlot slot, QWidget *widget) override;
    void bindServices(const ServiceBindings &services) override;

public slots:
    // GUI-thread presentation endpoint; no picking or numerical work is done here.
    void displayProbeResult(const QString &text);

signals:
    void viewRequested(const QString &orientation);
    void postProcessCommandRequested(const QString &command);

private:
    QWidget *createDataPanel();
    QWidget *createViewPanel();
    QWidget *createObjectPanel();
    QWidget *createViewportPanel();
    QWidget *createTimeline();
    void createActions();
    void openDeepSeekHarness();
    void showProbePanel();
    void openObjectStyle();
    void openObjectParameters();
    void openSelectedSettings();
    void updateSelectionActions();
    void showTaskPanel();
    void updateDataSelection(QTreeWidgetItem *item, int column);
    void addDerivedLayer(const QString &kind, const QString &name, const QString &message);
    void setFeatureActionsEnabled(bool mesh, bool contour, bool slice, bool iso, bool streamline);
    void appendMessage(const QString &message);
    void updateSelectionDetails();

    QVBoxLayout *viewportLayout_ = nullptr;
    QWidget *emptyViewport_ = nullptr;
    QTreeWidget *datasetTree_ = nullptr;
    QListWidget *sceneList_ = nullptr;
    QAction *styleAction_ = nullptr;
    QAction *parameterAction_ = nullptr;
    QAction *probeAction_ = nullptr;
    QPointer<QDialog> styleDialog_;
    QPointer<QDialog> parameterDialog_;
    QString selectedId_;
    QString selectedKind_;
    QString selectedName_;
    QHash<QString, QVariantMap> styleDrafts_;
    QHash<QString, QVariantMap> parameterDrafts_;
    int nextObjectId_ = 0;
    QSlider *timeSlider_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QLabel *sessionContext_ = nullptr;
    QLabel *viewportTitle_ = nullptr;
    QLabel *viewportHint_ = nullptr;
    QLabel *selectionSummary_ = nullptr;
    QTextEdit *probeResult_ = nullptr;
    QDockWidget *probeDock_ = nullptr;
    QDockWidget *taskDock_ = nullptr;
    QTextEdit *events_ = nullptr;
    QDockWidget *deepSeekHarnessDock_ = nullptr;
    QList<QAction *> featureActions_;
    ServiceBindings services_;
    bool bound_ = false;
};

} // namespace aerosim
