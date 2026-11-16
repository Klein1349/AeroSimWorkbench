#include "aerosim/ui/ParameterAssistantRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <cstddef>

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

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

// The executable doubles as the adapter process so tests need neither Node nor a desktop.
int runFakeAdapter(const QStringList &args)
{
    QFile script(args.value(1));
    if (!script.open(QIODevice::ReadOnly)) return 2;
    const QByteArray mode = script.readAll();
    QFile presetFile(args.value(2));
    if (!presetFile.open(QIODevice::ReadOnly)) return 3;
    const QJsonObject preset = QJsonDocument::fromJson(presetFile.readAll()).object();
    if (mode == "cancel") {
        QString cancelFile;
        for (const QString &arg : args)
            if (arg.startsWith(QStringLiteral("--cancel-file="))) cancelFile = arg.mid(14);
        for (int i = 0; i < 400 && !QFile::exists(cancelFile); ++i) QThread::msleep(5);
        QTextStream(stdout) << "{\"ok\":false,\"error\":{\"code\":\"CANCELLED\"}}\n";
        return 1;
    }
    QThread::msleep(100);
    if (mode == "invalid") {
        QTextStream(stdout) << "this is not a JSON result\n";
        return 0;
    }
    if (mode == "partial") {
        QTextStream(stdout) << "{\"ok\":false,\"before\":{\"field\":1},\"error\":{\"code\":\"VALUE_MISMATCH\"}}\n";
        return 1;
    }
    QJsonObject result{{QStringLiteral("ok"), true}, {QStringLiteral("verified"), true},
        {QStringLiteral("scope"), QStringLiteral("window-draft")},
        {QStringLiteral("page"), preset.value(QStringLiteral("page"))},
        {QStringLiteral("after"), mode == "false-success" ? QJsonValue(QJsonObject()) : preset.value(QStringLiteral("values"))},
        {QStringLiteral("verification"), QJsonObject{{QStringLiteral("focus_transfer"), true},
                                                   {QStringLiteral("page_round_trip"), true}}}};
    QFile output;
    output.open(stdout, QIODevice::WriteOnly);
    output.write(QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n');
    output.flush();
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1).endsWith(QStringLiteral("configure.mjs"))) return runFakeAdapter(app.arguments());
    const QByteArray oldRoot = qgetenv("AEROSIM_AGENT_DESKTOP_ROOT");
    const QByteArray oldNode = qgetenv("AEROSIM_NODE_EXECUTABLE");
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "Temporary adapter fixture failed")) return 1;
    const QString root = temporary.path();
    const QString tool = root + QStringLiteral("/tools/agent-desktop");
    QDir().mkpath(tool + QStringLiteral("/node_modules/agent-desktop"));
    if (!check(writeFile(tool + QStringLiteral("/node_modules/agent-desktop/package.json"), "{}"), "Tool marker write failed")) return 2;
    qputenv("AEROSIM_NODE_EXECUTABLE", QCoreApplication::applicationFilePath().toUtf8());

    aerosim::ParameterAssistantRunner runner;
    const QJsonObject preset{{QStringLiteral("page"), text(u"\u8ba1\u7b97\u63a7\u5236")},
        {QStringLiteral("values"), QJsonObject{{text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570"), 6}}}};
    bool finished = false;
    bool success = false;
    QString message;
    QString details;
    int runningTransitions = 0;
    QObject::connect(&runner, &aerosim::ParameterAssistantRunner::runningChanged,
                     [&runningTransitions](bool) { ++runningTransitions; });
    QObject::connect(&runner, &aerosim::ParameterAssistantRunner::finished,
                     [&](bool ok, const QString &text, const QString &raw) {
        finished = true; success = ok; message = text; details = raw;
    });
    qputenv("AEROSIM_AGENT_DESKTOP_ROOT", (root + QStringLiteral("/missing")).toUtf8());
    runner.start(preset);
    if (!check(finished && !success && !runner.isRunning() && runningTransitions == 0,
               "Missing adapter must fail before launching a process")) return 3;

    qputenv("AEROSIM_AGENT_DESKTOP_ROOT", root.toUtf8());
    const auto execute = [&](const QByteArray &mode, bool cancel) {
        finished = false;
        message.clear();
        details.clear();
        writeFile(tool + QStringLiteral("/configure.mjs"), mode);
        QEventLoop wait;
        QObject::connect(&runner, &aerosim::ParameterAssistantRunner::finished, &wait, &QEventLoop::quit);
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &wait, &QEventLoop::quit);
        watchdog.start(5000);
        runner.start(preset);
        if (cancel) QTimer::singleShot(25, &runner, [&runner] { runner.cancel(); });
        if (!finished) wait.exec();
        return finished && !runner.isRunning();
    };
    bool eventLoopResponsive = false;
    QTimer::singleShot(20, &app, [&eventLoopResponsive] { eventLoopResponsive = true; });
    const bool executed = execute("success", false);
    if (!executed || !success || !eventLoopResponsive || !message.contains(text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570 = 6")) || runningTransitions != 2)
        QTextStream(stderr) << "executed=" << executed << " success=" << success << " responsive=" << eventLoopResponsive
                            << " transitions=" << runningTransitions << " message=" << message << " details=" << details << '\n';
    if (!check(executed && success && eventLoopResponsive
               && message.contains(text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570 = 6")) && runningTransitions == 2,
               "Verified adapter result must complete asynchronously with readback values")) return 4;
    if (!check(execute("false-success", false) && !success,
               "A claimed success without matching requested values must be rejected")) return 5;
    if (!check(execute("partial", false) && !success && details.contains(QStringLiteral("VALUE_MISMATCH")),
               "Partial adapter failure must preserve diagnostics")) return 6;
    if (!check(execute("invalid", false) && !success && message.contains(text(u"\u6838\u5bf9")),
               "Malformed output must not be reported as successful or unchanged")) return 7;
    if (!check(execute("cancel", true) && !success && message.contains(text(u"\u4fdd\u7559")),
               "Cooperative cancellation must wait for adapter exit and disclose retained values")) return 8;
    qputenv("AEROSIM_AGENT_DESKTOP_ROOT", oldRoot);
    qputenv("AEROSIM_NODE_EXECUTABLE", oldNode);
    QTextStream(stdout) << "Parameter assistant async adapter, readback, failure and cancellation passed\n";
    return 0;
}
