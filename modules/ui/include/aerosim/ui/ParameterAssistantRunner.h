#pragma once

#include <QJsonObject>
#include <QByteArray>
#include <QObject>
#include <QString>
#include <memory>

class QProcess;
class QTemporaryDir;
class QTimer;

namespace aerosim {

// Runs the desktop adapter asynchronously; it never writes parameter widgets directly.
class ParameterAssistantRunner final : public QObject {
    Q_OBJECT
public:
    explicit ParameterAssistantRunner(QObject *parent = nullptr);
    ~ParameterAssistantRunner() override;

    bool isRunning() const { return running_; }
    void start(const QJsonObject &preset);
    void cancel();

signals:
    void runningChanged(bool running);
    void finished(bool success, const QString &message, const QString &details);

private:
    void requestCancellation(bool timedOut);
    void collectOutput();
    void handleFinished(int exitCode);
    void complete(bool success, const QString &message, const QString &details);
    void closeProcessJob();

    QProcess *process_ = nullptr;
    QTimer *deadline_ = nullptr;
    std::unique_ptr<QTemporaryDir> temporary_;
    QByteArray standardOutput_;
    QByteArray standardError_;
    QString cancellationPath_;
    QString readyPath_;
    QString startupFailure_;
    QJsonObject requestedPreset_;
    bool running_ = false;
    bool cancellationRequested_ = false;
    bool timedOut_ = false;
    bool outputTruncated_ = false;
    void *processJob_ = nullptr;
    void *processInformation_ = nullptr;
    bool processJobAttached_ = false;
};

} // namespace aerosim
