#pragma once

#include "aerosim/ui/IntegrationPorts.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QListWidget;
class QDockWidget;
class QSlider;
class QStackedWidget;
class QTextEdit;
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
    void appendMessage(const QString &message);
    void updateSelectionDetails();

    QVBoxLayout *viewportLayout_ = nullptr;
    QWidget *emptyViewport_ = nullptr;
    QListWidget *sceneList_ = nullptr;
    QStackedWidget *propertiesStack_ = nullptr;
    QLineEdit *selectionName_ = nullptr;
    QSlider *timeSlider_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QTextEdit *events_ = nullptr;
    QDockWidget *deepSeekHarnessDock_ = nullptr;
    ServiceBindings services_;
    bool bound_ = false;
};

} // namespace aerosim
