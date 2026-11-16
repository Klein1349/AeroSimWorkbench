#include "aerosim/ui/ParameterAiPlanner.h"
#include "aerosim/ui/ParameterCommandParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
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

QString fixtureKey()
{
    return QStringLiteral("synthetic-test-key-never-log");
}

QString fixtureInstruction()
{
    return text(u"\u628a\u9a6c\u8d6b\u6570\u8bbe\u4e3a0.8\uff0c\u653b\u89d25\u5ea6");
}

QJsonObject referenceCommand()
{
    return {{QStringLiteral("page"), text(u"\u53c2\u8003\u91cf")},
        {QStringLiteral("values"), QJsonObject{{text(u"\u9a6c\u8d6b\u6570"), 0.8},
                                              {text(u"\u653b\u89d2 (\u00b0)"), 5}}}};
}

QJsonObject fixturePlan()
{
    return {{QStringLiteral("summary"), text(u"\u8bbe\u7f6e\u6765\u6d41\u53c2\u6570")},
        {QStringLiteral("commands"), QJsonArray{referenceCommand()}}};
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

void writeJson(const QJsonObject &object)
{
    QFile output;
    output.open(stdout, QIODevice::WriteOnly);
    output.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    output.flush();
}

int fakePlanner(const QStringList &arguments)
{
    QFile script(arguments.value(1));
    if (!script.open(QIODevice::ReadOnly)) return 2;
    const QByteArray mode = script.readAll();
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) return 3;
    const QJsonDocument requestDocument = QJsonDocument::fromJson(input.readAll());
    const QJsonObject request = requestDocument.object();
    // Secrets belong in the private stdin request, never in process arguments.
    if (arguments.size() != 2 || arguments.join(QLatin1Char(' ')).contains(fixtureKey())
        || !requestDocument.isObject() || request.size() != 4
        || request.value(QStringLiteral("endpoint")).toString() != QStringLiteral("https://example.invalid/v1")
        || request.value(QStringLiteral("model")).toString() != QStringLiteral("test-model")
        || request.value(QStringLiteral("apiKey")).toString() != fixtureKey()
        || request.value(QStringLiteral("instruction")).toString() != fixtureInstruction()) {
        writeJson({{QStringLiteral("ok"), false}, {QStringLiteral("error"), QJsonObject{
            {QStringLiteral("message"), QStringLiteral("Fixture stdin/argument contract failed")}}}});
        return 4;
    }
    if (mode == "cancel") {
        QThread::msleep(5000);
        return 5;
    }
    QThread::msleep(100);
    if (mode == "invalid-json") {
        QFile output;
        output.open(stdout, QIODevice::WriteOnly);
        output.write("not a JSON response\n");
        output.flush();
        return 0;
    }
    if (mode == "error-secret") {
        QFile diagnostic;
        diagnostic.open(stderr, QIODevice::WriteOnly);
        diagnostic.write(fixtureKey().toUtf8());
        diagnostic.flush();
        writeJson({{QStringLiteral("ok"), false}, {QStringLiteral("error"), QJsonObject{
            {QStringLiteral("message"), QStringLiteral("Provider rejected ") + fixtureKey()}}}});
        return 1;
    }
    if (mode == "error-secret-boundary") {
        writeJson({{QStringLiteral("ok"), false}, {QStringLiteral("error"), QJsonObject{
            {QStringLiteral("message"), QString(1995, QLatin1Char('x')) + fixtureKey()}}}});
        return 1;
    }
    QJsonObject plan = fixturePlan();
    if (mode == "invalid-summary") plan.insert(QStringLiteral("summary"), 42);
    if (mode == "unknown-field") {
        QJsonObject command = referenceCommand();
        QJsonObject values = command.value(QStringLiteral("values")).toObject();
        values.insert(QStringLiteral("startSolver"), true);
        command.insert(QStringLiteral("values"), values);
        plan.insert(QStringLiteral("commands"), QJsonArray{command});
    }
    writeJson({{QStringLiteral("ok"), true}, {QStringLiteral("plan"), plan}});
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1).endsWith(QStringLiteral("ai-plan.mjs"))) return fakePlanner(app.arguments());
    QString error;
    if (!check(aerosim::validateParameterAiPlan(fixturePlan(), &error), "Known bounded parameter plan must be valid")) return 1;
    QJsonObject invalid = fixturePlan();
    invalid.insert(QStringLiteral("startSolver"), true);
    if (!check(!aerosim::validateParameterAiPlan(invalid, &error), "Extra plan actions must be rejected")) return 2;
    invalid = fixturePlan();
    invalid.insert(QStringLiteral("commands"), QJsonArray{referenceCommand(), referenceCommand()});
    if (!check(!aerosim::validateParameterAiPlan(invalid, &error), "Duplicate parameter pages must be rejected")) return 3;
    QJsonObject outOfRange = referenceCommand();
    outOfRange.insert(QStringLiteral("values"), QJsonObject{{text(u"\u9a6c\u8d6b\u6570"), -1}});
    invalid = fixturePlan();
    invalid.insert(QStringLiteral("commands"), QJsonArray{outOfRange});
    if (!check(!aerosim::validateParameterAiPlan(invalid, &error), "Out-of-range parameter values must be rejected")) return 4;
    invalid = fixturePlan();
    invalid.insert(QStringLiteral("commands"), QJsonArray());
    if (!check(!aerosim::validateParameterAiPlan(invalid, &error), "Empty command plans must be rejected")) return 5;
    const QJsonObject fractionalInteger{{QStringLiteral("page"), text(u"\u8ba1\u7b97\u63a7\u5236")},
        {QStringLiteral("values"), QJsonObject{{text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570"), 6.000000000000001}}}};
    invalid = fixturePlan();
    invalid.insert(QStringLiteral("commands"), QJsonArray{fractionalInteger});
    if (!check(!aerosim::validateParameterAiPlan(invalid, &error),
               "Near-integer AI values must be rejected without rounding")) return 15;
    QJsonObject excessivePrecision = referenceCommand();
    excessivePrecision.insert(QStringLiteral("values"), QJsonObject{{text(u"\u9a6c\u8d6b\u6570"), 1e-10}});
    invalid = fixturePlan();
    invalid.insert(QStringLiteral("commands"), QJsonArray{excessivePrecision});
    if (!check(!aerosim::validateParameterAiPlan(invalid, &error),
               "AI values below editor precision must be rejected without truncation")) return 16;
    const double largeValue = 999999999999.9999;
    const QString largeText = aerosim::parameterNumberText(largeValue);
    if (!check(largeText != QStringLiteral("1000000000000")
               && largeText.startsWith(QStringLiteral("999999999999.")) && largeText.toDouble() == largeValue,
               "Large parameter display must preserve the stored value instead of rounding to 15 significant digits")) return 17;

    const QByteArray oldRoot = qgetenv("AEROSIM_AGENT_DESKTOP_ROOT");
    const QByteArray oldNode = qgetenv("AEROSIM_NODE_EXECUTABLE");
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "Planner fixture directory failed")) return 6;
    const QString tool = temporary.path() + QStringLiteral("/tools/agent-desktop");
    if (!check(QDir().mkpath(tool), "Planner fixture path creation failed")) return 7;
    qputenv("AEROSIM_AGENT_DESKTOP_ROOT", temporary.path().toUtf8());
    qputenv("AEROSIM_NODE_EXECUTABLE", QCoreApplication::applicationFilePath().toUtf8());
    const aerosim::ParameterAiSettings settings{QStringLiteral("https://example.invalid/v1"), QStringLiteral("test-model"), fixtureKey()};
    aerosim::ParameterAiPlanner planner;
    bool completed = false;
    bool success = false;
    QJsonObject plan;
    QString message;
    int transitions = 0;
    int completions = 0;
    QObject::connect(&planner, &aerosim::ParameterAiPlanner::runningChanged,
                     [&transitions](bool) { ++transitions; });
    QObject::connect(&planner, &aerosim::ParameterAiPlanner::finished,
                     [&](bool ok, const QJsonObject &result, const QString &status) {
        completed = true; success = ok; plan = result; message = status; ++completions;
    });
    planner.start(QString(), settings);
    if (!check(completed && !success && !planner.isRunning() && transitions == 0,
               "An empty instruction must fail without starting the planner")) return 8;

    const auto execute = [&](const QByteArray &mode, bool cancel) {
        completed = false;
        plan = {};
        message.clear();
        if (!writeFile(tool + QStringLiteral("/ai-plan.mjs"), mode)) return false;
        QEventLoop wait;
        QObject::connect(&planner, &aerosim::ParameterAiPlanner::finished, &wait, &QEventLoop::quit);
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &wait, &QEventLoop::quit);
        watchdog.start(7000);
        planner.start(fixtureInstruction(), settings);
        if (cancel) QTimer::singleShot(50, &planner, [&planner] { planner.cancel(); });
        if (!completed) wait.exec();
        return completed && !planner.isRunning();
    };
    bool responsive = false;
    QTimer::singleShot(25, &app, [&responsive] { responsive = true; });
    if (!check(execute("success", false) && success && responsive && plan == fixturePlan() && transitions == 2,
               "Planner must send stdin request and return a validated plan asynchronously")) return 9;
    if (!check(execute("unknown-field", false) && !success && plan.isEmpty(),
               "Untrusted model fields must be rejected before returning a plan")) return 10;
    if (!check(execute("error-secret", false) && !success && plan.isEmpty()
               && !message.contains(fixtureKey()) && message.contains(QStringLiteral("[redacted]")),
               "Adapter errors must redact the key and discard stderr")) return 11;
    if (!check(execute("invalid-json", false) && !success && plan.isEmpty(),
               "Invalid adapter output must not become an executable plan")) return 12;
    if (!check(execute("invalid-summary", false) && !success && plan.isEmpty(),
               "A non-string summary must be rejected before normalization")) return 18;
    if (!check(execute("error-secret-boundary", false) && !success && plan.isEmpty()
               && !message.contains(fixtureKey().left(5)) && message.size() <= 2000,
               "Credentials must be redacted before truncating long error text")) return 19;
    if (!check(execute("cancel", true) && !success && plan.isEmpty() && message.contains(text(u"\u53d6\u6d88")),
               "Cancelled planner requests must finish without a plan")) return 13;
    if (!check(transitions == 14 && completions == 8, "Each request must emit exactly one completion and paired running transitions")) return 14;
    qputenv("AEROSIM_AGENT_DESKTOP_ROOT", oldRoot);
    qputenv("AEROSIM_NODE_EXECUTABLE", oldNode);
    QTextStream(stdout) << "AI planner stdin transport, bounded validation, credential redaction and cancellation passed\n";
    return 0;
}
