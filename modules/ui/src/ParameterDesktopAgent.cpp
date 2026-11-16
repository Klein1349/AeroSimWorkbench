#include "aerosim/ui/ParameterDesktopAgent.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
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
template <std::size_t N>
QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

const qint64 kMaximumOutputBytes = 8 * 1024 * 1024;
const int kMaximumLineBytes = 256 * 1024;
const int kMaximumLogs = 1000;
const int kMaximumMessage = 4000;
const int kMaximumDetail = 16000;

QString scriptPath()
{
    const QString configured = qEnvironmentVariable("AEROSIM_AGENT_DESKTOP_ROOT");
    QDir directory(configured.isEmpty() ? QCoreApplication::applicationDirPath() : configured);
    do {
        const QString candidate = directory.filePath(QStringLiteral("tools/agent-desktop/ai-agent.mjs"));
        if (QFileInfo(candidate).isFile()) return candidate;
        if (!configured.isEmpty()) break;
    } while (directory.cdUp());
    return {};
}

QString nodeExecutable()
{
    const QString configured = qEnvironmentVariable("AEROSIM_NODE_EXECUTABLE");
    if (!configured.isEmpty()) {
        const QFileInfo executable(configured);
        return executable.isFile() && executable.isExecutable() ? executable.absoluteFilePath() : QString();
    }
    QString node = QStandardPaths::findExecutable(QStringLiteral("node"));
#ifdef Q_OS_WIN
    if (node.isEmpty()) {
        const QFileInfo candidate(QDir(qEnvironmentVariable("ProgramFiles")).filePath(QStringLiteral("nodejs/node.exe")));
        if (candidate.isFile()) node = candidate.absoluteFilePath();
    }
#endif
    return node;
}

QString detailText(const QJsonValue &value)
{
    if (value.isString()) return value.toString();
    if (value.isObject()) return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray()) return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return {};
}
} // namespace

ParameterDesktopAgent::ParameterDesktopAgent(QObject *parent) : QObject(parent)
{
    process_ = new QProcess(this);
    deadline_ = new QTimer(this);
    deadline_->setSingleShot(true);
    cancelDeadline_ = new QTimer(this);
    cancelDeadline_->setSingleShot(true);
    connect(deadline_, &QTimer::timeout, this, [this] {
        timedOut_ = true;
        emit logMessage(QStringLiteral("warning"), text(u"\u684c\u9762\u4f1a\u8bdd\u5df2\u8fbe\u5230\u65f6\u95f4\u4e0a\u9650\uff0c\u6b63\u5728\u505c\u6b62\u3002"), {});
        forceStop();
    });
    connect(cancelDeadline_, &QTimer::timeout, this, &ParameterDesktopAgent::forceStop);
    connect(process_, &QProcess::started, this, &ParameterDesktopAgent::handleStarted);
    connect(process_, &QProcess::readyReadStandardOutput, this, &ParameterDesktopAgent::collectOutput);
    connect(process_, &QProcess::readyReadStandardError, this, [this] { process_->readAllStandardError(); });
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int exitCode, QProcess::ExitStatus status) { handleFinished(exitCode, status == QProcess::NormalExit); });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && running_)
            complete(false, text(u"\u684c\u9762\u52a9\u624b\u65e0\u6cd5\u542f\u52a8\uff0c\u8bf7\u68c0\u67e5 Node.js \u5b89\u88c5\u3002"));
    });
#ifdef Q_OS_WIN
    process_->setCreateProcessArgumentsModifier([this](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
        processInformation_ = arguments->processInformation;
    });
#endif
}

ParameterDesktopAgent::~ParameterDesktopAgent()
{
    deadline_->stop();
    cancelDeadline_->stop();
    disconnect(process_, nullptr, this, nullptr);
    if (process_->state() != QProcess::NotRunning) {
        closeProcessJob();
        process_->kill();
        process_->waitForFinished(1500);
    }
    closeProcessJob();
    request_.fill('\0');
    pendingOutput_.fill('\0');
    key_.fill(QChar(0));
}

void ParameterDesktopAgent::start(const QString &instruction, const ParameterAiSettings &settings)
{
    if (running_) return;
    if (instruction.trimmed().isEmpty() || instruction.size() > 8192) {
        emit finished(false, text(u"\u8bf7\u8f93\u5165 8192 \u5b57\u4ee5\u5185\u7684\u5de5\u4f5c\u53f0\u64cd\u4f5c\u8981\u6c42\u3002"));
        return;
    }
    if (!hasParameterAiSettings(settings)) {
        emit finished(false, text(u"\u8bf7\u5148\u5728 AI \u8bbe\u7f6e\u4e2d\u586b\u5199\u6709\u6548\u7684\u63a5\u53e3\u5730\u5740\u3001\u6a21\u578b\u548c API Key\u3002"));
        return;
    }
    const QString script = scriptPath();
    const QString node = nodeExecutable();
    if (script.isEmpty() || node.isEmpty()) {
        emit finished(false, text(u"\u627e\u4e0d\u5230\u684c\u9762\u52a9\u624b\u7a0b\u5e8f\u6216 Node.js\uff0c\u8bf7\u68c0\u67e5\u5de5\u5177\u90e8\u7f72\u3002"));
        return;
    }
    temporary_.reset(new QTemporaryDir(QDir::tempPath() + QStringLiteral("/aerosim-desktop-agent-XXXXXX")));
    if (!temporary_->isValid()) {
        temporary_.reset();
        emit finished(false, text(u"\u65e0\u6cd5\u521b\u5efa\u684c\u9762\u4f1a\u8bdd\u4e34\u65f6\u76ee\u5f55\u3002"));
        return;
    }
    readyPath_ = temporary_->filePath(QStringLiteral("ready"));
    cancelPath_ = temporary_->filePath(QStringLiteral("cancel"));
    key_ = settings.apiKey;
    request_ = QJsonDocument(QJsonObject{{QStringLiteral("endpoint"), settings.endpoint}, {QStringLiteral("model"), settings.model},
        {QStringLiteral("apiKey"), settings.apiKey}, {QStringLiteral("instruction"), instruction},
        {QStringLiteral("targetPid"), double(QCoreApplication::applicationPid())},
        {QStringLiteral("readyFile"), readyPath_}, {QStringLiteral("cancelFile"), cancelPath_}}).toJson(QJsonDocument::Compact);
    pendingOutput_.clear();
    resultMessage_.clear();
    startupFailure_.clear();
    processInformation_ = nullptr;
    outputBytes_ = 0;
    logCount_ = 0;
    cancellationRequested_ = timedOut_ = protocolFailed_ = resultSeen_ = resultSuccess_ = logLimitReported_ = false;
    closeProcessJob();
#ifdef Q_OS_WIN
    processJob_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!processJob_ || !SetInformationJobObject(HANDLE(processJob_), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        complete(false, text(u"\u65e0\u6cd5\u5efa\u7acb\u684c\u9762\u4f1a\u8bdd\u8fdb\u7a0b\u4fdd\u62a4\uff0c\u672a\u542f\u52a8\u64cd\u4f5c\u3002"));
        return;
    }
#endif
    process_->setWorkingDirectory(QFileInfo(script).absolutePath());
    process_->setProgram(node);
    process_->setArguments({script});
    running_ = true;
    emit runningChanged(true);
    deadline_->start(10 * 60 * 1000);
    process_->start();
}

void ParameterDesktopAgent::handleStarted()
{
#ifdef Q_OS_WIN
    const auto *information = static_cast<const PROCESS_INFORMATION *>(processInformation_);
    if (!processJob_ || !information || !information->hProcess
        || !AssignProcessToJobObject(HANDLE(processJob_), information->hProcess)) {
        startupFailure_ = text(u"\u65e0\u6cd5\u4fdd\u62a4\u684c\u9762\u4f1a\u8bdd\u5b50\u8fdb\u7a0b\uff0c\u672a\u542f\u52a8\u64cd\u4f5c\u3002");
        forceStop();
        return;
    }
#endif
    QSaveFile ready(readyPath_);
    if (!ready.open(QIODevice::WriteOnly) || ready.write("ready\n") != 6 || !ready.commit()) {
        startupFailure_ = text(u"\u65e0\u6cd5\u51c6\u5907\u684c\u9762\u4f1a\u8bdd\uff0c\u672a\u542f\u52a8\u64cd\u4f5c\u3002");
        forceStop();
        return;
    }
    if (process_->write(request_) != request_.size()) {
        startupFailure_ = text(u"\u65e0\u6cd5\u53d1\u9001\u684c\u9762\u4f1a\u8bdd\u8bf7\u6c42\u3002");
        forceStop();
        return;
    }
    process_->closeWriteChannel();
    request_.fill('\0');
    request_.clear();
}

void ParameterDesktopAgent::cancel()
{
    if (!running_ || cancellationRequested_) return;
    cancellationRequested_ = true;
    QSaveFile cancellation(cancelPath_);
    if (!cancellation.open(QIODevice::WriteOnly) || cancellation.write("cancel\n") != 7 || !cancellation.commit()) {
        forceStop();
        return;
    }
    emit logMessage(QStringLiteral("info"), text(u"\u5df2\u8bf7\u6c42\u505c\u6b62\uff0c\u7b49\u5f85\u5f53\u524d\u64cd\u4f5c\u7ed3\u675f\u3002"), {});
    cancelDeadline_->start(30000);
}

QString ParameterDesktopAgent::safeText(const QString &value, int limit) const
{
    QString safe = value;
    if (!key_.isEmpty()) {
        safe.replace(key_, QStringLiteral("[redacted]"));
        const QByteArray encoded = QJsonDocument(QJsonArray{key_}).toJson(QJsonDocument::Compact);
        const QString escaped = QString::fromUtf8(encoded.mid(2, encoded.size() - 4));
        if (escaped != key_) safe.replace(escaped, QStringLiteral("[redacted]"));
    }
    static const QRegularExpression bearer(QStringLiteral("\\bBearer\\s+[^\\s\"'<>]+"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression keyPattern(QStringLiteral("\\bsk-[A-Za-z0-9_-]{8,}\\b"));
    safe.replace(bearer, QStringLiteral("Bearer [redacted]"));
    safe.replace(keyPattern, QStringLiteral("[redacted]"));
    // Redact the complete field before truncating so a key crossing the limit cannot leak a prefix.
    return safe.left(limit);
}

void ParameterDesktopAgent::collectOutput()
{
    const QByteArray chunk = process_->readAllStandardOutput();
    if (!running_ || protocolFailed_) return;
    outputBytes_ += chunk.size();
    if (outputBytes_ > kMaximumOutputBytes) { failProtocol(); return; }
    pendingOutput_.append(chunk);
    int newline;
    while ((newline = pendingOutput_.indexOf('\n')) >= 0) {
        if (newline > kMaximumLineBytes) { failProtocol(); return; }
        const QByteArray line = pendingOutput_.left(newline);
        pendingOutput_.remove(0, newline + 1);
        consumeLine(line);
        if (protocolFailed_) return;
    }
    if (pendingOutput_.size() > kMaximumLineBytes) failProtocol();
}

void ParameterDesktopAgent::consumeLine(const QByteArray &line)
{
    if (line.trimmed().isEmpty()) return;
    if (resultSeen_) { failProtocol(); return; }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { failProtocol(); return; }
    const QJsonObject object = document.object();
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("result")) {
        if (!object.value(QStringLiteral("ok")).isBool() || !object.value(QStringLiteral("message")).isString()) { failProtocol(); return; }
        resultSeen_ = true;
        resultSuccess_ = object.value(QStringLiteral("ok")).toBool();
        resultMessage_ = safeText(object.value(QStringLiteral("message")).toString(), kMaximumMessage);
        return;
    }
    if (type != QStringLiteral("log") || !object.value(QStringLiteral("level")).isString()
        || !object.value(QStringLiteral("message")).isString()) { failProtocol(); return; }
    if (logCount_ >= kMaximumLogs) {
        if (!logLimitReported_) {
            logLimitReported_ = true;
            emit logMessage(QStringLiteral("warning"), text(u"\u65e5\u5fd7\u5df2\u8fbe\u5230\u663e\u793a\u4e0a\u9650\uff0c\u540e\u7eed\u6b65\u9aa4\u4ecd\u4f1a\u7ee7\u7eed\u3002"), {});
        }
        return;
    }
    ++logCount_;
    emit logMessage(safeText(object.value(QStringLiteral("level")).toString(), 32),
        safeText(object.value(QStringLiteral("message")).toString(), kMaximumMessage),
        safeText(detailText(object.value(QStringLiteral("detail"))), kMaximumDetail));
}

void ParameterDesktopAgent::failProtocol()
{
    protocolFailed_ = true;
    pendingOutput_.fill('\0');
    pendingOutput_.clear();
    forceStop();
}

void ParameterDesktopAgent::forceStop()
{
    closeProcessJob();
    if (process_->state() != QProcess::NotRunning) process_->kill();
}

void ParameterDesktopAgent::handleFinished(int exitCode, bool normalExit)
{
    if (!running_) return;
    collectOutput();
    if (!pendingOutput_.trimmed().isEmpty() && !protocolFailed_) {
        const QByteArray line = pendingOutput_;
        pendingOutput_.clear();
        consumeLine(line);
    }
    if (!startupFailure_.isEmpty()) { complete(false, startupFailure_); return; }
    if (timedOut_) { complete(false, text(u"\u684c\u9762\u4f1a\u8bdd\u5df2\u8d85\u65f6\u505c\u6b62\uff0c\u5df2\u6267\u884c\u7684\u64cd\u4f5c\u4f1a\u4fdd\u7559\u3002")); return; }
    if (cancellationRequested_) { complete(false, text(u"\u5df2\u505c\u6b62\u684c\u9762\u4f1a\u8bdd\uff0c\u5df2\u6267\u884c\u7684\u64cd\u4f5c\u4f1a\u4fdd\u7559\u3002")); return; }
    if (protocolFailed_ || !resultSeen_) { complete(false, text(u"\u684c\u9762\u52a9\u624b\u8fd4\u56de\u65e0\u6548\u6216\u4e0d\u5b8c\u6574\u7ed3\u679c\uff0c\u8bf7\u6838\u5bf9\u5f53\u524d\u5de5\u4f5c\u53f0\u3002")); return; }
    const bool success = normalExit && exitCode == 0 && resultSuccess_;
    if (!normalExit || (exitCode != 0 && resultSuccess_)) {
        complete(false, text(u"\u684c\u9762\u52a9\u624b\u5f02\u5e38\u7ed3\u675f\uff0c\u8bf7\u6838\u5bf9\u5df2\u6267\u884c\u7684\u64cd\u4f5c\u3002"));
        return;
    }
    complete(success, resultMessage_.isEmpty() ? (success ? text(u"\u684c\u9762\u4efb\u52a1\u5df2\u5b8c\u6210\u3002") : text(u"\u684c\u9762\u4efb\u52a1\u672a\u5b8c\u6210\uff0c\u8bf7\u67e5\u770b\u65e5\u5fd7\u3002")) : resultMessage_);
}

void ParameterDesktopAgent::complete(bool success, const QString &message)
{
    deadline_->stop();
    cancelDeadline_->stop();
    closeProcessJob();
    const QString safeMessage = safeText(message, kMaximumMessage);
    request_.fill('\0'); request_.clear();
    pendingOutput_.fill('\0'); pendingOutput_.clear();
    key_.fill(QChar(0)); key_.clear();
    temporary_.reset();
    readyPath_.clear(); cancelPath_.clear();
    processInformation_ = nullptr;
    const bool wasRunning = running_;
    running_ = false;
    if (wasRunning) emit runningChanged(false);
    emit finished(success, safeMessage);
}

void ParameterDesktopAgent::closeProcessJob()
{
#ifdef Q_OS_WIN
    if (processJob_) CloseHandle(HANDLE(processJob_));
#endif
    processJob_ = nullptr;
}

} // namespace aerosim
