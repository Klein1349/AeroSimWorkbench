#pragma once

#include "aerosim/ui/ParameterAiSettings.h"
#include <QByteArray>
#include <QObject>
#include <QString>
#include <memory>

class QProcess;
class QTemporaryDir;
class QTimer;

namespace aerosim {

// Streams an AI desktop session while keeping its process tree and secrets local.
class ParameterDesktopAgent final : public QObject {
    Q_OBJECT
public:
    explicit ParameterDesktopAgent(QObject *parent = nullptr);
    ~ParameterDesktopAgent() override;
    bool isRunning() const { return running_; }
    void start(const QString &instruction, const ParameterAiSettings &settings);
    void cancel();

signals:
    void runningChanged(bool running);
    void logMessage(const QString &level, const QString &message, const QString &detail);
    void finished(bool success, const QString &message);

private:
    void handleStarted();
    void collectOutput();
    void consumeLine(const QByteArray &line);
    void handleFinished(int exitCode, bool normalExit);
    void failProtocol();
    void forceStop();
    void complete(bool success, const QString &message);
    void closeProcessJob();
    QString safeText(const QString &value, int limit) const;

    QProcess *process_ = nullptr;
    QTimer *deadline_ = nullptr;
    QTimer *cancelDeadline_ = nullptr;
    std::unique_ptr<QTemporaryDir> temporary_;
    QByteArray request_;
    QByteArray pendingOutput_;
    QString key_;
    QString readyPath_;
    QString cancelPath_;
    QString startupFailure_;
    QString resultMessage_;
    qint64 outputBytes_ = 0;
    int logCount_ = 0;
    bool running_ = false;
    bool cancellationRequested_ = false;
    bool timedOut_ = false;
    bool protocolFailed_ = false;
    bool resultSeen_ = false;
    bool resultSuccess_ = false;
    bool logLimitReported_ = false;
    void *processJob_ = nullptr;
    void *processInformation_ = nullptr;
};

} // namespace aerosim
