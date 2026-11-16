#include "aerosim/ui/ParameterAiPlanner.h"
#include "aerosim/ui/ParameterCommandParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <cstddef>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace aerosim {
namespace {
template <std::size_t N> QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

QString plannerPath()
{
    const QString configured = qEnvironmentVariable("AEROSIM_AGENT_DESKTOP_ROOT");
    QDir directory(configured.isEmpty() ? QCoreApplication::applicationDirPath() : configured);
    do {
        const QString path = directory.filePath(QStringLiteral("tools/agent-desktop/ai-plan.mjs"));
        if (QFileInfo(path).isFile()) return path;
        if (!configured.isEmpty()) break;
    } while (directory.cdUp());
    return {};
}

QString nodeExecutable()
{
    const QString configured = qEnvironmentVariable("AEROSIM_NODE_EXECUTABLE");
    if (!configured.isEmpty()) return QFileInfo(configured).isFile() ? configured : QString();
    QString path = QStandardPaths::findExecutable(QStringLiteral("node"));
#ifdef Q_OS_WIN
    if (path.isEmpty()) {
        const QString candidate = QDir(qEnvironmentVariable("ProgramFiles")).filePath(QStringLiteral("nodejs/node.exe"));
        if (QFileInfo(candidate).isFile()) path = candidate;
    }
#endif
    return path;
}
} // namespace

bool validateParameterAiPlan(const QJsonObject &plan, QString *error)
{
    const auto reject = [error](const QString &message) { if (error) *error = message; return false; };
    if (error) error->clear();
    if (plan.size() != 2 || !plan.value("summary").isString() || !plan.value("commands").isArray()
        || plan.value("summary").toString().trimmed().isEmpty() || plan.value("summary").toString().size() > 2048)
        return reject(text(u"AI\u8fd4\u56de\u7684\u65b9\u6848\u683c\u5f0f\u65e0\u6548\uff0c\u672a\u4fee\u6539\u53c2\u6570\u3002"));
    const auto commands = plan.value("commands").toArray();
    if (commands.isEmpty() || commands.size() > 2) return reject(text(u"AI\u65b9\u6848\u7684\u9875\u9762\u6570\u91cf\u65e0\u6548\u3002"));
    QSet<QString> pages;
    for (const auto &value : commands) {
        if (!value.isObject()) return reject(text(u"AI\u65b9\u6848\u4e2d\u5b58\u5728\u65e0\u6548\u53c2\u6570\u9879\u3002"));
        const QJsonObject command = value.toObject();
        if (!validateParameterPreset(command, error)) return false;
        const QString page = command.value("page").toString();
        if (pages.contains(page)) return reject(text(u"AI\u65b9\u6848\u5305\u542b\u91cd\u590d\u9875\u9762\uff0c\u8bf7\u91cd\u65b0\u63cf\u8ff0\u8bf7\u6c42\u3002"));
        pages.insert(page);
    }
    return true;
}

ParameterAiPlanner::ParameterAiPlanner(QObject *parent) : QObject(parent)
{
    process_ = new QProcess(this);
    deadline_ = new QTimer(this);
    deadline_->setSingleShot(true);
#ifdef Q_OS_WIN
    process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#endif
    connect(deadline_, &QTimer::timeout, this, [this] { timedOut_ = true; process_->kill(); });
    connect(process_, &QProcess::started, this, [this] {
        process_->write(request_);
        process_->closeWriteChannel();
        request_.fill('\0');
        request_.clear();
    });
    connect(process_, &QProcess::readyReadStandardOutput, this, &ParameterAiPlanner::collectOutput);
    // Never expose provider/debug stderr; it may contain credentials or a request body.
    connect(process_, &QProcess::readyReadStandardError, this, [this] { process_->readAllStandardError(); });
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus) { handleFinished(exitCode); });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && running_)
            complete(false, {}, text(u"AI\u8fde\u63a5\u7a0b\u5e8f\u65e0\u6cd5\u542f\u52a8\uff0c\u8bf7\u68c0\u67e5Node.js\u5b89\u88c5\u3002"));
    });
}

ParameterAiPlanner::~ParameterAiPlanner()
{
    disconnect(process_, nullptr, this, nullptr);
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(1500);
    }
}

void ParameterAiPlanner::start(const QString &instruction, const ParameterAiSettings &settings)
{
    if (running_) return;
    if (instruction.trimmed().isEmpty() || instruction.size() > 2048) {
        emit finished(false, {}, text(u"\u8bf7\u8f93\u51652048\u5b57\u4ee5\u5185\u7684\u53c2\u6570\u8bf7\u6c42\u3002"));
        return;
    }
    if (settings.apiKey.isEmpty() || settings.model.isEmpty() || settings.endpoint.isEmpty()) {
        emit finished(false, {}, text(u"\u8bf7\u5148\u5728AI\u8bbe\u7f6e\u4e2d\u586b\u5199\u63a5\u53e3\u5730\u5740\u3001\u6a21\u578b\u548cAPI Key\u3002"));
        return;
    }
    const QString script = plannerPath();
    const QString node = nodeExecutable();
    if (script.isEmpty() || node.isEmpty()) {
        emit finished(false, {}, text(u"\u627e\u4e0d\u5230AI\u8fde\u63a5\u7a0b\u5e8f\u6216Node.js\uff0c\u8bf7\u68c0\u67e5\u684c\u9762\u5de5\u5177\u90e8\u7f72\u3002"));
        return;
    }
    key_ = settings.apiKey;
    request_ = QJsonDocument(QJsonObject{{"endpoint", settings.endpoint}, {"model", settings.model},
        {"apiKey", settings.apiKey}, {"instruction", instruction}}).toJson(QJsonDocument::Compact);
    output_.clear();
    cancelled_ = timedOut_ = tooLarge_ = false;
    process_->setProgram(node);
    process_->setArguments({script});
    process_->setWorkingDirectory(QFileInfo(script).absolutePath());
    running_ = true;
    emit runningChanged(true);
    deadline_->start(120000);
    process_->start();
}

void ParameterAiPlanner::cancel()
{
    if (!running_) return;
    cancelled_ = true;
    process_->kill();
}

void ParameterAiPlanner::collectOutput()
{
    const QByteArray chunk = process_->readAllStandardOutput();
    if (output_.size() + chunk.size() > 1024 * 1024) {
        tooLarge_ = true;
        process_->kill();
        return;
    }
    output_.append(chunk);
}

void ParameterAiPlanner::handleFinished(int exitCode)
{
    if (!running_) return;
    collectOutput();
    if (cancelled_ || timedOut_) {
        complete(false, {}, timedOut_ ? text(u"AI\u8bf7\u6c42\u8d85\u65f6\uff0c\u672a\u6267\u884c\u53c2\u6570\u4fee\u6539\u3002") : text(u"\u5df2\u53d6\u6d88AI\u8bf7\u6c42\uff0c\u672a\u6267\u884c\u53c2\u6570\u4fee\u6539\u3002"));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output_, &parseError);
    if (tooLarge_ || parseError.error != QJsonParseError::NoError || !document.isObject()) {
        complete(false, {}, text(u"AI\u8fde\u63a5\u7a0b\u5e8f\u672a\u8fd4\u56de\u6709\u6548\u7ed3\u679c\uff0c\u672a\u4fee\u6539\u53c2\u6570\u3002"));
        return;
    }
    const auto result = document.object();
    if (exitCode == 0 && result.value("ok").toBool() && result.value("plan").isObject()) {
        auto plan = result.value("plan").toObject();
        QString error;
        if (!validateParameterAiPlan(plan, &error)) {
            complete(false, {}, error);
            return;
        }
        QString summary = plan.value("summary").toString();
        if (!key_.isEmpty()) summary.replace(key_, QStringLiteral("[redacted]"));
        plan.insert(QStringLiteral("summary"), summary);
        complete(true, plan, text(u"AI\u53c2\u6570\u65b9\u6848\u5df2\u751f\u6210\u3002"));
        return;
    }
    const auto error = result.value("error").toObject();
    QString message = error.value("message").toString();
    if (message.isEmpty()) message = text(u"AI\u8bf7\u6c42\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u63a5\u53e3\u3001\u6a21\u578b\u548cKey\u662f\u5426\u5339\u914d\u5f53\u524d\u6e20\u9053\u3002");
    complete(false, {}, message);
}

void ParameterAiPlanner::complete(bool success, const QJsonObject &plan, const QString &message)
{
    deadline_->stop();
    QString safeMessage = message;
    if (!key_.isEmpty()) safeMessage.replace(key_, QStringLiteral("[redacted]"));
    safeMessage = safeMessage.left(2000);
    output_.fill('\0'); output_.clear();
    request_.fill('\0'); request_.clear();
    key_.clear();
    const bool wasRunning = running_;
    running_ = false;
    if (wasRunning) emit runningChanged(false);
    emit finished(success, plan, safeMessage);
}
} // namespace aerosim
