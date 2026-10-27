#pragma once

#include "aerosim/ui/IntegrationPorts.h"

#include <QList>
#include <QMainWindow>

class QLabel;
class QLineEdit;
class QListWidget;
class QDockWidget;
class QSlider;
class QStackedWidget;
class QTextEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QWidget;

namespace aerosim {

// The post-processing workspace owns Qt controls only. A visualization scene
// service supplies the actual VTK widget through WidgetSlot::Viewport.
class PostProcessWorkbenchWindow final : public QMainWindow, public IWidgetHost {
    Q_OBJECT
public:
    explicit PostProcessWorkbenchWindow(QWidget *parent = nullptr);

    QWidget *windowWidget() override { return this; }
    bool insertWidget(WidgetSlot slot, QWidget *widget) override;
    void bindServices(const ServiceBindings &services) override;

signals:
    void viewRequested(const QString &orientation);
    void postProcessCommandRequested(const QString &command);

private:
    QWidget *createDataPanel();
    QWidget *createScenePanel();
    QWidget *createViewportPanel();
    QWidget *createPropertiesPanel();
    QWidget *createTimeline();
    void createActions();
    void openDeepSeekHarness();
    void showProbePanel();
    void updateDataSelection(QTreeWidgetItem *item, int column);
    void addDerivedLayer(const QString &kind, const QString &name, const QString &message);
    void setFeatureButtonsEnabled(bool mesh, bool contour, bool slice, bool iso, bool streamline);
    void appendMessage(const QString &message);
    void updateSelectionDetails();

    QVBoxLayout *viewportLayout_ = nullptr;
    QWidget *emptyViewport_ = nullptr;
    QTreeWidget *datasetTree_ = nullptr;
    QListWidget *sceneList_ = nullptr;
    QStackedWidget *propertiesStack_ = nullptr;
    QLineEdit *selectionName_ = nullptr;
    QSlider *timeSlider_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QLabel *sessionContext_ = nullptr;
    QLabel *viewportTitle_ = nullptr;
    QLabel *viewportHint_ = nullptr;
    QLabel *propertyContextTitle_ = nullptr;
    QLabel *propertyContextHint_ = nullptr;
    QLabel *selectionSummary_ = nullptr;
    QLabel *zoneSummary_ = nullptr;
    QLabel *fieldSummary_ = nullptr;
    QLabel *timeSummary_ = nullptr;
    QLabel *curveSummary_ = nullptr;
    QLabel *derivedSummary_ = nullptr;
    QLabel *probeSummary_ = nullptr;
    QTextEdit *events_ = nullptr;
    QDockWidget *deepSeekHarnessDock_ = nullptr;
    QList<QToolButton *> featureButtons_;
    ServiceBindings services_;
    bool bound_ = false;
};

} // namespace aerosim
