#pragma once

#include "aerosim/ui/ParameterAiSettings.h"
#include <QByteArray>
#include <QJsonObject>
#include <QObject>

class QProcess;
class QTimer;

namespace aerosim {
bool validateParameterAiPlan(const QJsonObject &plan, QString *error = nullptr);

// A model proposes values; this object never operates the desktop.
class ParameterAiPlanner final : public QObject {
    Q_OBJECT
public:
    explicit ParameterAiPlanner(QObject *parent = nullptr);
    ~ParameterAiPlanner() override;
    bool isRunning() const { return running_; }
    void start(const QString &instruction, const ParameterAiSettings &settings);
    void cancel();

signals:
    void runningChanged(bool running);
    void finished(bool success, const QJsonObject &plan, const QString &message);

private:
    void collectOutput();
    void handleFinished(int exitCode);
    void complete(bool success, const QJsonObject &plan, const QString &message);
    QProcess *process_ = nullptr;
    QTimer *deadline_ = nullptr;
    QByteArray output_;
    QByteArray request_;
    QString key_;
    bool running_ = false;
    bool cancelled_ = false;
    bool timedOut_ = false;
    bool tooLarge_ = false;
};
} // namespace aerosim
