#include "aerosim/ui/ParameterAssistantRunner.h"
#include "aerosim/ui/ParameterCommandParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
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

const int kOutputLimit = 1024 * 1024;

bool isToolRoot(const QString &path)
{
    const QDir root(path);
    return QFileInfo(root.filePath(QStringLiteral("tools/agent-desktop/configure.mjs"))).isFile()
        && QFileInfo(root.filePath(QStringLiteral("tools/agent-desktop/node_modules/agent-desktop/package.json"))).isFile();
}

QString findToolRoot()
{
    const QString configured = qEnvironmentVariable("AEROSIM_AGENT_DESKTOP_ROOT");
    if (!configured.isEmpty()) return isToolRoot(configured) ? QDir(configured).absolutePath() : QString();
    QDir directory(QCoreApplication::applicationDirPath());
    do {
        if (isToolRoot(directory.absolutePath())) return directory.absolutePath();
    } while (directory.cdUp());
    return {};
}

QString findNode()
{
    const QString configured = qEnvironmentVariable("AEROSIM_NODE_EXECUTABLE");
    if (!configured.isEmpty()) {
        const QFileInfo executable(configured);
        return executable.isFile() && executable.isExecutable() ? executable.absoluteFilePath() : QString();
    }
    QString executable = QStandardPaths::findExecutable(QStringLiteral("node"));
#ifdef Q_OS_WIN
    if (executable.isEmpty()) {
        const QFileInfo candidate(QDir(qEnvironmentVariable("ProgramFiles")).filePath(QStringLiteral("nodejs/node.exe")));
        if (candidate.isFile()) executable = candidate.absoluteFilePath();
    }
#endif
    return executable;
}

QString verifiedValues(const QJsonObject &after)
{
    QStringList values;
    for (auto it = after.begin(); it != after.end(); ++it) {
        const QString value = it.value().isDouble() ? parameterNumberText(it.value().toDouble())
            : it.value().isBool() ? (it.value().toBool() ? text(u"\u5f00\u542f") : text(u"\u5173\u95ed"))
            : it.value().toString();
        values.append(it.key() + QStringLiteral(" = ") + value);
    }
    return values.join(text(u"\uff1b"));
}

QString failureMessage(const QString &code)
{
    if (code == QStringLiteral("CANCELLED")) return text(u"\u5df2\u505c\u6b62\u914d\u7f6e\uff1b\u5df2\u5199\u5165\u7684\u503c\u4f1a\u4fdd\u7559\uff0c\u8bf7\u6838\u5bf9\u5f53\u524d\u53c2\u6570\u3002");
    if (code == QStringLiteral("AMBIGUOUS_WINDOW") || code == QStringLiteral("WINDOW_CHANGED"))
        return text(u"\u65e0\u6cd5\u786e\u8ba4\u5f53\u524d\u5de5\u4f5c\u53f0\u7a97\u53e3\uff0c\u8bf7\u5173\u95ed\u91cd\u590d\u7a97\u53e3\u540e\u91cd\u8bd5\u3002");
    if (code == QStringLiteral("WINDOW_UNAVAILABLE"))
        return text(u"\u5de5\u4f5c\u53f0\u5f53\u524d\u4e0d\u53ef\u64cd\u4f5c\uff0c\u8bf7\u6062\u590d\u7a97\u53e3\u5e76\u5173\u95ed\u5f39\u7a97\u540e\u91cd\u8bd5\u3002");
    if (code == QStringLiteral("MISSING_DEPENDENCY"))
        return text(u"\u5c1a\u672a\u5b89\u88c5\u684c\u9762\u5de5\u5177\uff0c\u8bf7\u8fd0\u884c scripts/setup-agent-desktop.ps1\u3002");
    if (code == QStringLiteral("NAVIGATION_FAILED"))
        return text(u"\u672a\u80fd\u6253\u5f00\u76ee\u6807\u53c2\u6570\u9875\uff1b\u8bf7\u624b\u52a8\u6253\u5f00\u8be5\u9875\u540e\u91cd\u8bd5\u3002");
    if (code == QStringLiteral("FIELD_CLIPPED") || code == QStringLiteral("CONTROL_OUTSIDE_WINDOW"))
        return text(u"\u76ee\u6807\u53c2\u6570\u672a\u5b8c\u6574\u663e\u793a\uff0c\u8bf7\u653e\u5927\u7a97\u53e3\u6216\u6eda\u52a8\u5230\u8be5\u53c2\u6570\u540e\u91cd\u8bd5\u3002");
    if (code == QStringLiteral("DRAFT_VERIFICATION_FAILED") || code == QStringLiteral("VALUE_MISMATCH"))
        return text(u"\u53c2\u6570\u56de\u8bfb\u4e0e\u6307\u4ee4\u4e0d\u4e00\u81f4\uff0c\u8bf7\u6838\u5bf9\u5f53\u524d\u9875\u9762\u3002");
    if (code == QStringLiteral("UNSUPPORTED_FIELD") || code == QStringLiteral("INVALID_VALUE")
        || code == QStringLiteral("INVALID_PRESET"))
        return text(u"\u6307\u4ee4\u4e2d\u7684\u53c2\u6570\u6216\u6570\u503c\u4e0d\u53d7\u652f\u6301\uff0c\u8bf7\u68c0\u67e5\u540e\u91cd\u8bd5\u3002");
    return text(u"\u914d\u7f6e\u672a\u5b8c\u6210\uff0c\u90e8\u5206\u503c\u53ef\u80fd\u5df2\u5199\u5165\uff1b\u8bf7\u6838\u5bf9\u53c2\u6570\u540e\u91cd\u8bd5\u3002");
}
} // namespace

ParameterAssistantRunner::ParameterAssistantRunner(QObject *parent) : QObject(parent)
{
    process_ = new QProcess(this);
    deadline_ = new QTimer(this);
    deadline_->setSingleShot(true);
    connect(deadline_, &QTimer::timeout, this, [this] { requestCancellation(true); });
    connect(process_, &QProcess::readyReadStandardOutput, this, &ParameterAssistantRunner::collectOutput);
    connect(process_, &QProcess::readyReadStandardError, this, &ParameterAssistantRunner::collectOutput);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus) { handleFinished(exitCode); });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && running_)
            complete(false, text(u"\u53c2\u6570\u52a9\u624b\u542f\u52a8\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5 Node.js \u5b89\u88c5\u3002"), process_->errorString());
    });
#ifdef Q_OS_WIN
    process_->setCreateProcessArgumentsModifier([this](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
        processInformation_ = arguments->processInformation;
    });
#endif
    connect(process_, &QProcess::started, this, [this] {
#ifdef Q_OS_WIN
        // The handle supplied by Qt identifies the exact process, even if a PID is reused.
        const auto *information = static_cast<const PROCESS_INFORMATION *>(processInformation_);
        if (!processJob_ || !information || !information->hProcess
            || !AssignProcessToJobObject(HANDLE(processJob_), information->hProcess)) {
            startupFailure_ = QStringLiteral("Could not attach the adapter to its process job (Windows error %1). Desktop access was not enabled.")
                .arg(GetLastError());
            process_->kill();
            return;
        }
        processJobAttached_ = true;
#endif
        // The helper must not access the desktop before this atomic readiness marker appears.
        QSaveFile ready(readyPath_);
        if (!ready.open(QIODevice::WriteOnly) || ready.write("ready\n") != 6 || !ready.commit()) {
            startupFailure_ = QStringLiteral("Could not write the adapter readiness marker: ") + ready.errorString();
            process_->kill();
        }
    });
}

ParameterAssistantRunner::~ParameterAssistantRunner()
{
    deadline_->stop();
    disconnect(process_, nullptr, this, nullptr);
    if (process_->state() != QProcess::NotRunning) {
        requestCancellation(false);
        closeProcessJob();
        process_->kill();
        process_->waitForFinished(1500);
    }
    closeProcessJob();
}

void ParameterAssistantRunner::start(const QJsonObject &preset)
{
    if (running_) return;
    const QString root = findToolRoot();
    if (root.isEmpty()) {
        emit finished(false, text(u"\u627e\u4e0d\u5230\u684c\u9762\u5de5\u5177\uff0c\u8bf7\u8fd0\u884c scripts/setup-agent-desktop.ps1\u3002"),
                      QStringLiteral("Expected tools/agent-desktop/configure.mjs and installed node_modules in an application-directory ancestor or AEROSIM_AGENT_DESKTOP_ROOT."));
        return;
    }
    const QString node = findNode();
    if (node.isEmpty()) {
        emit finished(false, text(u"\u627e\u4e0d\u5230 Node.js\uff0c\u8bf7\u5b89\u88c5 Node.js 18 \u6216\u66f4\u65b0\u7248\u672c\u3002"),
                      QStringLiteral("Set AEROSIM_NODE_EXECUTABLE to the node executable if it is not on PATH."));
        return;
    }
    temporary_.reset(new QTemporaryDir(QDir::tempPath() + QStringLiteral("/aerosim-parameters-XXXXXX")));
    if (!temporary_->isValid()) {
        emit finished(false, text(u"\u65e0\u6cd5\u521b\u5efa\u4e34\u65f6\u53c2\u6570\u6587\u4ef6\uff0c\u8bf7\u68c0\u67e5\u4e34\u65f6\u76ee\u5f55\u6743\u9650\u3002"), temporary_->errorString());
        return;
    }
    QFile input(temporary_->filePath(QStringLiteral("preset.json")));
    const QByteArray json = QJsonDocument(preset).toJson(QJsonDocument::Compact);
    if (!input.open(QIODevice::WriteOnly) || input.write(json) != json.size()) {
        emit finished(false, text(u"\u65e0\u6cd5\u5199\u5165\u4e34\u65f6\u53c2\u6570\u6587\u4ef6\u3002"), input.errorString());
        temporary_.reset();
        return;
    }
    input.close();
    cancellationPath_ = temporary_->filePath(QStringLiteral("cancel"));
    readyPath_ = temporary_->filePath(QStringLiteral("ready"));
    startupFailure_.clear();
    processInformation_ = nullptr;
    requestedPreset_ = preset;
    standardOutput_.clear();
    standardError_.clear();
    cancellationRequested_ = false;
    timedOut_ = false;
    outputTruncated_ = false;
    closeProcessJob();
#ifdef Q_OS_WIN
    processJob_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!processJob_ || !SetInformationJobObject(HANDLE(processJob_), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        const QString details = QStringLiteral("Could not create the adapter process job (Windows error %1). No adapter was started.")
            .arg(GetLastError());
        complete(false, text(u"\u53c2\u6570\u52a9\u624b\u65e0\u6cd5\u5efa\u7acb\u8fdb\u7a0b\u4fdd\u62a4\uff0c\u672a\u4fee\u6539\u53c2\u6570\u3002"), details);
        return;
    }
#endif
    process_->setWorkingDirectory(root);
    process_->setProgram(node);
    process_->setArguments({QDir(root).filePath(QStringLiteral("tools/agent-desktop/configure.mjs")), input.fileName(),
        QStringLiteral("--headed"), QStringLiteral("--target-pid=%1").arg(QCoreApplication::applicationPid()),
        QStringLiteral("--cancel-file=") + cancellationPath_, QStringLiteral("--ready-file=") + readyPath_});
    running_ = true;
    emit runningChanged(true);
    deadline_->start(180000);
    process_->start();
}

void ParameterAssistantRunner::cancel()
{
    requestCancellation(false);
}

void ParameterAssistantRunner::requestCancellation(bool timedOut)
{
    if (!running_ || cancellationRequested_) return;
    QFile cancellation(cancellationPath_);
    if (cancellation.open(QIODevice::WriteOnly)) {
        cancellation.write("cancel\n");
        cancellation.close();
        cancellationRequested_ = true;
        timedOut_ = timedOut;
    }
}

void ParameterAssistantRunner::collectOutput()
{
    const auto append = [this](QByteArray &target, const QByteArray &chunk) {
        const int remaining = kOutputLimit - target.size();
        target.append(chunk.left(qMax(0, remaining)));
        if (chunk.size() > remaining) outputTruncated_ = true;
    };
    append(standardOutput_, process_->readAllStandardOutput());
    append(standardError_, process_->readAllStandardError());
}

void ParameterAssistantRunner::handleFinished(int exitCode)
{
    if (!running_) return;
    collectOutput();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(standardOutput_, &parseError);
    const QString details = QString::fromUtf8(standardOutput_)
        + (standardError_.isEmpty() ? QString() : QStringLiteral("\nStandard error:\n") + QString::fromUtf8(standardError_));
    if (!startupFailure_.isEmpty()) {
        complete(false, text(u"\u53c2\u6570\u52a9\u624b\u542f\u52a8\u4fdd\u62a4\u5931\u8d25\uff0c\u672a\u4fee\u6539\u53c2\u6570\u3002"), startupFailure_ + QStringLiteral("\n") + details);
        return;
    }
    if (outputTruncated_ || parseError.error != QJsonParseError::NoError || !document.isObject()) {
        complete(false, text(u"\u672a\u6536\u5230\u6709\u6548\u7684\u914d\u7f6e\u7ed3\u679c\uff1b\u90e8\u5206\u503c\u53ef\u80fd\u5df2\u5199\u5165\uff0c\u8bf7\u6838\u5bf9\u53c2\u6570\u3002"), details);
        return;
    }
    const QJsonObject result = document.object();
    const QJsonObject after = result.value(QStringLiteral("after")).toObject();
    const QJsonObject requested = requestedPreset_.value(QStringLiteral("values")).toObject();
    bool valuesMatch = !requested.isEmpty();
    for (auto it = requested.begin(); it != requested.end(); ++it)
        valuesMatch = valuesMatch && after.contains(it.key()) && after.value(it.key()) == it.value();
    const QJsonObject verification = result.value(QStringLiteral("verification")).toObject();
    const bool success = exitCode == 0 && result.value(QStringLiteral("ok")).toBool()
        && result.value(QStringLiteral("verified")).toBool()
        && result.value(QStringLiteral("scope")).toString() == QStringLiteral("window-draft")
        && result.value(QStringLiteral("page")) == requestedPreset_.value(QStringLiteral("page"))
        && verification.value(QStringLiteral("focus_transfer")).toBool()
        && verification.value(QStringLiteral("page_round_trip")).toBool() && valuesMatch;
    if (success) {
        const QString values = verifiedValues(result.value(QStringLiteral("after")).toObject());
        complete(true, text(u"\u5df2\u914d\u7f6e\u5e76\u56de\u8bfb\u9a8c\u8bc1\uff1a") + values + text(u"\uff08\u5f53\u524d\u7a97\u53e3\u8349\u7a3f\uff09"), details);
        return;
    }
    if (timedOut_) {
        complete(false, text(u"\u914d\u7f6e\u5df2\u8d85\u65f6\u5e76\u505c\u6b62\uff1b\u5df2\u5199\u5165\u7684\u503c\u4f1a\u4fdd\u7559\uff0c\u8bf7\u6838\u5bf9\u5f53\u524d\u53c2\u6570\u3002"), details);
        return;
    }
    const QString code = result.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString();
    complete(false, failureMessage(code), details);
}

void ParameterAssistantRunner::complete(bool success, const QString &message, const QString &details)
{
    deadline_->stop();
    closeProcessJob();
    temporary_.reset();
    cancellationPath_.clear();
    readyPath_.clear();
    processInformation_ = nullptr;
    const bool wasRunning = running_;
    running_ = false;
    if (wasRunning) emit runningChanged(false);
    emit finished(success, message, details);
}

void ParameterAssistantRunner::closeProcessJob()
{
#ifdef Q_OS_WIN
    if (processJob_) CloseHandle(HANDLE(processJob_));
#endif
    processJob_ = nullptr;
    processJobAttached_ = false;
}

} // namespace aerosim
