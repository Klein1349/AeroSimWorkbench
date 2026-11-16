#include "aerosim/ui/ParameterDesktopAgent.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <cstddef>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
template <std::size_t N>
QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

bool check(bool passed, const char *message)
{
    if (!passed) QTextStream(stderr) << message << '\n';
    return passed;
}

QString fixtureKey()
{
    return QStringLiteral("synthetic-desktop-key-never-log");
}

QString fixtureInstruction()
{
    return text(u"\u6253\u5f00\u521d\u59cb\u5316\u8bbe\u7f6e\u5e76\u68c0\u67e5\u7f51\u683c\u8fd0\u52a8\u53c2\u6570");
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

void writeJson(QFile &output, const QJsonObject &object, bool splitUtf8 = false)
{
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    if (splitUtf8) {
        int at = 0;
        while (at < bytes.size() && uchar(bytes.at(at)) < 0x80) ++at;
        const int split = qMin(bytes.size(), at + 1);
        output.write(bytes.left(split));
        output.flush();
        QThread::msleep(15);
        output.write(bytes.mid(split));
    } else output.write(bytes);
    output.flush();
}

int fakeAgent(const QStringList &arguments)
{
    QFile script(arguments.value(1));
    if (!script.open(QIODevice::ReadOnly)) return 2;
    const QByteArray mode = script.readAll();
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) return 3;
    const auto request = QJsonDocument::fromJson(input.readAll()).object();
    QFile output;
    output.open(stdout, QIODevice::WriteOnly);
    if (arguments.size() != 2 || arguments.join(QLatin1Char(' ')).contains(fixtureKey()) || request.size() != 7
        || request.value(QStringLiteral("endpoint")).toString() != QStringLiteral("https://example.invalid/v1")
        || request.value(QStringLiteral("model")).toString() != QStringLiteral("test-model")
        || request.value(QStringLiteral("apiKey")).toString() != fixtureKey()
        || request.value(QStringLiteral("instruction")).toString() != fixtureInstruction()
        || request.value(QStringLiteral("targetPid")).toDouble() <= 0
        || request.value(QStringLiteral("targetPid")).toDouble() == double(QCoreApplication::applicationPid())
        || !QFile::exists(request.value(QStringLiteral("readyFile")).toString())
        || request.value(QStringLiteral("cancelFile")).toString().isEmpty()) {
        writeJson(output, {{QStringLiteral("type"), QStringLiteral("result")}, {QStringLiteral("ok"), false},
            {QStringLiteral("message"), QStringLiteral("fixture transport or readiness contract failed")}});
        return 4;
    }
    const auto log = [&](const QString &message, const QJsonValue &detail = QJsonValue()) {
        writeJson(output, {{QStringLiteral("type"), QStringLiteral("log")}, {QStringLiteral("level"), QStringLiteral("info")},
            {QStringLiteral("message"), message}, {QStringLiteral("detail"), detail}});
    };
    const auto result = [&](bool ok, const QString &message) {
        writeJson(output, {{QStringLiteral("type"), QStringLiteral("result")}, {QStringLiteral("ok"), ok},
            {QStringLiteral("message"), message}});
    };
    if (mode == "cancel") {
        for (int i = 0; i < 1000 && !QFile::exists(request.value(QStringLiteral("cancelFile")).toString()); ++i)
            QThread::msleep(5);
        if (!QFile::exists(request.value(QStringLiteral("cancelFile")).toString())) return 5;
        log(QStringLiteral("cooperative-cancel-observed"));
        result(false, QStringLiteral("cancelled"));
        return 0;
    }
    if (mode == "spawn-child") {
        QProcess child;
#ifdef Q_OS_WIN
        child.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) { arguments->flags |= CREATE_NO_WINDOW; });
#endif
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--fake-mcp")});
        if (!child.waitForStarted(2000)) return 6;
        log(QStringLiteral("fixture-child"), QString::number(child.processId()));
        QThread::msleep(5000);
        return 0;
    }
    QThread::msleep(80);
    if (mode == "invalid") {
        output.write("invalid JSON " + fixtureKey().toUtf8() + '\n');
        output.flush();
        return 0;
    }
    if (mode == "oversized") {
        output.write(QByteArray(256 * 1024 + 1, 'x') + fixtureKey().toUtf8() + '\n');
        output.flush();
        return 0;
    }
    if (mode == "error") {
        QFile diagnostic;
        diagnostic.open(stderr, QIODevice::WriteOnly);
        diagnostic.write(fixtureKey().toUtf8());
        diagnostic.flush();
        result(false, QStringLiteral("provider error ") + fixtureKey());
        return 1;
    }
    if (mode == "missing-result") {
        log(QStringLiteral("step finished"));
        return 0;
    }
    if (mode == "duplicate-result") {
        result(true, QStringLiteral("done"));
        result(true, QStringLiteral("done again"));
        return 0;
    }
    if (mode == "abnormal-exit") {
        result(true, QStringLiteral("claimed success"));
        return 7;
    }
    if (mode == "log-limit") {
        for (int i = 0; i < 1005; ++i) log(QStringLiteral("bounded log"));
        result(true, QStringLiteral("done"));
        return 0;
    }
    writeJson(output, {{QStringLiteral("type"), QStringLiteral("log")}, {QStringLiteral("level"), QStringLiteral("info")},
        {QStringLiteral("message"), text(u"\u8bfb\u53d6\u754c\u9762")},
        {QStringLiteral("detail"), QJsonObject{{QStringLiteral("phase"), QStringLiteral("observe")}}}}, true);
    log(QString(3995, QLatin1Char('x')) + fixtureKey(), QString(15995, QLatin1Char('d')) + fixtureKey());
    result(true, QString(3995, QLatin1Char('r')) + fixtureKey());
    return 0;
}

struct LogEntry {
    QString level;
    QString message;
    QString detail;
};
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1) == QStringLiteral("--fake-mcp")) { QThread::msleep(5000); return 0; }
    if (app.arguments().value(1).endsWith(QStringLiteral("ai-agent.mjs"))) return fakeAgent(app.arguments());
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "Agent fixture directory failed")) return 1;
    const QString tool = temporary.path() + QStringLiteral("/tools/agent-desktop");
    if (!check(QDir().mkpath(tool), "Agent fixture path creation failed")) return 2;
    const QByteArray oldRoot = qgetenv("AEROSIM_AGENT_DESKTOP_ROOT");
    const QByteArray oldNode = qgetenv("AEROSIM_NODE_EXECUTABLE");
    qputenv("AEROSIM_AGENT_DESKTOP_ROOT", temporary.path().toUtf8());
    qputenv("AEROSIM_NODE_EXECUTABLE", QCoreApplication::applicationFilePath().toUtf8());
    const aerosim::ParameterAiSettings settings{QStringLiteral("https://example.invalid/v1"), QStringLiteral("test-model"), fixtureKey()};
    aerosim::ParameterDesktopAgent agent;
    QVector<LogEntry> logs;
    bool completed = false;
    bool success = false;
    QString message;
    int transitions = 0;
    int completions = 0;
    QObject::connect(&agent, &aerosim::ParameterDesktopAgent::runningChanged, [&transitions](bool) { ++transitions; });
    QObject::connect(&agent, &aerosim::ParameterDesktopAgent::logMessage,
        [&](const QString &level, const QString &value, const QString &detail) { logs.append({level, value, detail}); });
    QObject::connect(&agent, &aerosim::ParameterDesktopAgent::finished,
        [&](bool ok, const QString &value) { completed = true; success = ok; message = value; ++completions; });
    agent.start(QString(), settings);
    if (!check(completed && !success && !agent.isRunning() && transitions == 0,
               "Invalid instructions must fail without launching a process")) return 3;
    const auto execute = [&](const QByteArray &mode, bool cancel = false) {
        completed = false;
        success = false;
        message.clear();
        logs.clear();
        if (!writeFile(tool + QStringLiteral("/ai-agent.mjs"), mode)) return false;
        QEventLoop wait;
        QObject::connect(&agent, &aerosim::ParameterDesktopAgent::finished, &wait, &QEventLoop::quit);
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &wait, &QEventLoop::quit);
        watchdog.start(8000);
        agent.start(fixtureInstruction(), settings);
        if (cancel) QTimer::singleShot(50, &agent, [&agent] { agent.cancel(); });
        if (!completed) wait.exec();
        return completed && !agent.isRunning();
    };
    bool responsive = false;
    QTimer::singleShot(20, &app, [&responsive] { responsive = true; });
    if (!check(execute("success") && success && responsive && logs.size() == 2
               && logs.first().message == text(u"\u8bfb\u53d6\u754c\u9762") && transitions == 2,
               "Agent must stream complete UTF-8 log lines and finish asynchronously through private stdin")) return 4;
    const QString sensitivePrefix = fixtureKey().left(12);
    if (!check(message.size() <= 4000 && !message.contains(sensitivePrefix),
               "Result keys crossing the truncation boundary must be redacted before clipping")) return 5;
    for (const LogEntry &entry : logs)
        if (!check(entry.message.size() <= 4000 && entry.detail.size() <= 16000
                   && !entry.message.contains(sensitivePrefix) && !entry.detail.contains(sensitivePrefix),
                   "Log keys crossing field limits must never leak a prefix")) return 6;
    if (!check(execute("error") && !success && !message.contains(fixtureKey())
               && message.contains(QStringLiteral("[redacted]")), "Errors must redact keys and discard provider stderr")) return 7;
    if (!check(execute("cancel", true) && !success && message.contains(text(u"\u505c\u6b62")),
               "Cancellation must finish after the adapter observes the private cancellation file")) return 8;
    bool observedCancellation = false;
    for (const LogEntry &entry : logs) observedCancellation |= entry.message == QStringLiteral("cooperative-cancel-observed");
    if (!check(observedCancellation, "The adapter did not observe cooperative cancellation")) return 9;
    for (const QByteArray mode : {QByteArray("invalid"), QByteArray("oversized"), QByteArray("missing-result"), QByteArray("duplicate-result"), QByteArray("abnormal-exit")})
        if (!check(execute(mode) && !success && !message.contains(sensitivePrefix),
                   "Malformed, incomplete, duplicated and abnormal agent results must fail without raw output")) return 10;
    if (!check(execute("log-limit") && success && logs.size() == 1001,
               "Agent logs must be bounded without hiding the final outcome")) return 11;
    if (!check(transitions == 18 && completions == 10, "Every request must finish once with paired busy transitions")) return 12;
#ifdef Q_OS_WIN
    // A fake MCP child proves the job owns descendants, even when the owner is destroyed.
    writeFile(tool + QStringLiteral("/ai-agent.mjs"), "spawn-child");
    auto *cleanupAgent = new aerosim::ParameterDesktopAgent;
    QEventLoop childWait;
    qint64 childPid = 0;
    QObject::connect(cleanupAgent, &aerosim::ParameterDesktopAgent::logMessage, &childWait,
        [&](const QString &, const QString &value, const QString &detail) {
            if (value == QStringLiteral("fixture-child")) { childPid = detail.toLongLong(); childWait.quit(); }
        });
    QTimer::singleShot(4000, &childWait, &QEventLoop::quit);
    cleanupAgent->start(fixtureInstruction(), settings);
    childWait.exec();
    const HANDLE child = childPid > 0 ? OpenProcess(SYNCHRONIZE, FALSE, DWORD(childPid)) : nullptr;
    delete cleanupAgent;
    const bool childStopped = child && WaitForSingleObject(child, 1500) == WAIT_OBJECT_0;
    if (child) CloseHandle(child);
    if (!check(childStopped, "Destroying a desktop session must stop its fake MCP descendant")) return 13;
#endif
    qputenv("AEROSIM_AGENT_DESKTOP_ROOT", oldRoot);
    qputenv("AEROSIM_NODE_EXECUTABLE", oldNode);
    QTextStream(stdout) << "Desktop agent private transport, stream logs, redaction, cancellation and process cleanup passed\n";
    return 0;
}
