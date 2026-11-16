#pragma once

#include "aerosim/ui/ParameterAiSettings.h"
#include <QWidget>
#include <QPointer>
#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QAction;
class QDockWidget;
class QPlainTextEdit;

namespace aerosim {
class ParameterAssistantRunner;
class ParameterDesktopAgent;

class ParameterAssistantPanel final : public QWidget {
    Q_OBJECT
public:
    explicit ParameterAssistantPanel(QWidget *parent = nullptr);
    explicit ParameterAssistantPanel(std::function<ParameterAiSettings()> settingsLoader, QWidget *parent = nullptr);
    bool isRunning() const;
    void resetLogLayout();

signals:
    void runningChanged(bool running);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void execute();
    void stopExecution();
    void openAiSettings();
    void setAiMode(bool enabled);
    void refreshBusyState();
    void ensureLogWindow();
    void placeLogDockAtBottom();
    void showExecutionLog();
    void appendLog(const QString &level, const QString &message, const QString &detail = QString());
    QString redact(const QString &message) const;
    void setStatus(const QString &message, bool error = false);

    ParameterAssistantRunner *runner_ = nullptr;
    ParameterDesktopAgent *agent_ = nullptr;
    QLineEdit *command_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *execute_ = nullptr;
    QPushButton *cancel_ = nullptr;
    QPushButton *details_ = nullptr;
    QPushButton *settings_ = nullptr;
    QPushButton *log_ = nullptr;
    QPointer<QWidget> logWindow_;
    QPointer<QDockWidget> logDock_;
    QPointer<QPlainTextEdit> logOutput_;
    QAction *aiModeAction_ = nullptr;
    QAction *localModeAction_ = nullptr;
    std::function<ParameterAiSettings()> settingsLoader_;
    QString lastDetails_;
    QString statusText_;
    QString activeKey_;
    bool aiMode_ = true;
    bool lastBusy_ = false;
    bool stopRequested_ = false;
    bool logDefaultSizePending_ = false;
};
} // namespace aerosim
