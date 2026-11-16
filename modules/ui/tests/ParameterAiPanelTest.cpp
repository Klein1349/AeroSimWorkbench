#include "aerosim/ui/ParameterAiSettings.h"
#include "aerosim/ui/ParameterAssistantPanel.h"
#include "aerosim/ui/ParameterAssistantRunner.h"
#include "aerosim/ui/ParameterDesktopAgent.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <cstddef>
#include <functional>

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

class EnvironmentValue final {
public:
    EnvironmentValue(const char *name, const QByteArray &value)
        : name_(name), previous_(qgetenv(name)), existed_(qEnvironmentVariableIsSet(name))
    {
        qputenv(name, value);
    }
    ~EnvironmentValue()
    {
        if (existed_) qputenv(name_.constData(), previous_);
        else qunsetenv(name_.constData());
    }
private:
    QByteArray name_;
    QByteArray previous_;
    bool existed_;
};

QString fixtureInstruction()
{
    return text(u"\u6253\u5f00\u7f51\u683c\u8fd0\u52a8\u5f39\u7a97\uff0c\u5e76\u6838\u5bf9\u5f53\u524d\u9009\u9879");
}

aerosim::ParameterAiSettings fixtureSettings()
{
    return {QStringLiteral("https://example.invalid/v1"), QStringLiteral("test-model"),
        QStringLiteral("synthetic-panel-fixture-key")};
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool appendFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Append) && file.write(content) == content.size();
}

void writeJson(const QJsonObject &object)
{
    QFile output;
    output.open(stdout, QIODevice::WriteOnly);
    output.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    output.flush();
}

int fakeAgent(const QStringList &arguments)
{
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) return 2;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll());
    const QJsonObject request = document.object();
    // The injected settings contain only a synthetic credential. Never log requests or keys.
    if (arguments.size() != 2 || !document.isObject()
        || request.value(QStringLiteral("endpoint")).toString() != fixtureSettings().endpoint
        || request.value(QStringLiteral("model")).toString() != fixtureSettings().model
        || request.value(QStringLiteral("apiKey")).toString() != fixtureSettings().apiKey
        || request.value(QStringLiteral("instruction")).toString() != fixtureInstruction()
        || request.value(QStringLiteral("targetPid")).toVariant().toLongLong() != qEnvironmentVariable("AEROSIM_PANEL_TEST_PID").toLongLong()
        || !QFile::exists(request.value(QStringLiteral("readyFile")).toString())
        || request.value(QStringLiteral("cancelFile")).toString().isEmpty()) {
        writeJson({{QStringLiteral("type"), QStringLiteral("result")}, {QStringLiteral("ok"), false},
            {QStringLiteral("message"), QStringLiteral("Fixture request contract failed")}});
        return 3;
    }
    const QString directory = QFileInfo(arguments.value(1)).absolutePath();
    if (!appendFile(directory + QStringLiteral("/agent.log"), "agent\n")) return 4;
    const QByteArray mode = readFile(arguments.value(1));
    writeJson({{QStringLiteral("type"), QStringLiteral("log")}, {QStringLiteral("level"), QStringLiteral("info")},
        {QStringLiteral("message"), QStringLiteral("desktop.snapshot <b>literal tool output</b>")},
        {QStringLiteral("detail"), QStringLiteral("fixture-tool-detail ") + fixtureSettings().apiKey}});
    for (int i = 0; i < (mode == "cancel" ? 600 : 60); ++i) {
        if (QFile::exists(request.value(QStringLiteral("cancelFile")).toString())) {
            writeJson({{QStringLiteral("type"), QStringLiteral("result")}, {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Fixture stopped; existing changes retained")},
                {QStringLiteral("cancelled"), true}});
            return 1;
        }
        QThread::msleep(5);
    }
    if (mode == "error") {
        writeJson({{QStringLiteral("type"), QStringLiteral("log")}, {QStringLiteral("level"), QStringLiteral("error")},
            {QStringLiteral("message"), QStringLiteral("Fixture desktop action failed")},
            {QStringLiteral("detail"), QStringLiteral("tool.click fixture failure")}});
        writeJson({{QStringLiteral("type"), QStringLiteral("result")}, {QStringLiteral("ok"), false},
            {QStringLiteral("message"), QStringLiteral("Fixture desktop action failed")}});
        return 1;
    }
    if (!writeFile(directory + QStringLiteral("/agent-state"), "verified unrestricted dialog task")) return 5;
    writeJson({{QStringLiteral("type"), QStringLiteral("result")}, {QStringLiteral("ok"), true},
        {QStringLiteral("message"), QStringLiteral("Fixture desktop dialog verified")}});
    return 0;
}

int fakeLocalAdapter(const QStringList &arguments)
{
    QString ready;
    QString cancel;
    for (const QString &argument : arguments) {
        if (argument.startsWith(QStringLiteral("--ready-file="))) ready = argument.mid(13);
        if (argument.startsWith(QStringLiteral("--cancel-file="))) cancel = argument.mid(14);
    }
    for (int i = 0; i < 400 && !QFile::exists(ready); ++i) QThread::msleep(5);
    if (!QFile::exists(ready)) return 2;
    const QJsonDocument document = QJsonDocument::fromJson(readFile(arguments.value(2)));
    if (!document.isObject()) return 3;
    const QJsonObject preset = document.object();
    const QString directory = QFileInfo(arguments.value(1)).absolutePath();
    if (!appendFile(directory + QStringLiteral("/local.log"), QJsonDocument(preset).toJson(QJsonDocument::Compact) + '\n')) return 4;
    if (readFile(arguments.value(1)) == "cancel") {
        for (int i = 0; i < 600 && !QFile::exists(cancel); ++i) QThread::msleep(5);
        writeJson({{QStringLiteral("ok"), false}, {QStringLiteral("error"), QJsonObject{
            {QStringLiteral("code"), QStringLiteral("CANCELLED")}}}});
        return 1;
    }
    QThread::msleep(100);
    writeJson({{QStringLiteral("ok"), true}, {QStringLiteral("verified"), true},
        {QStringLiteral("scope"), QStringLiteral("window-draft")},
        {QStringLiteral("page"), preset.value(QStringLiteral("page"))},
        {QStringLiteral("after"), preset.value(QStringLiteral("values"))},
        {QStringLiteral("verification"), QJsonObject{{QStringLiteral("focus_transfer"), true},
                                                   {QStringLiteral("page_round_trip"), true}}}});
    return 0;
}

bool waitFor(const std::function<bool()> &completed)
{
    if (completed()) return true;
    QEventLoop loop;
    QTimer poll;
    QTimer watchdog;
    bool timedOut = false;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (completed()) loop.quit(); });
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        for (QWidget *widget : QApplication::topLevelWidgets())
            if (auto *dialog = qobject_cast<QDialog *>(widget)) dialog->reject();
        loop.quit();
    });
    watchdog.setSingleShot(true);
    watchdog.start(7000);
    poll.start(5);
    loop.exec();
    return !timedOut && completed();
}
} // namespace

int main(int argc, char **argv)
{
    const QString script = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    if (script.endsWith(QStringLiteral("ai-agent.mjs")) || script.endsWith(QStringLiteral("configure.mjs"))) {
        QCoreApplication child(argc, argv);
        return script.endsWith(QStringLiteral("ai-agent.mjs")) ? fakeAgent(child.arguments()) : fakeLocalAdapter(child.arguments());
    }
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "Panel fixture directory failed")) return 1;
    const QString tool = temporary.path() + QStringLiteral("/tools/agent-desktop");
    const QString agentPath = tool + QStringLiteral("/ai-agent.mjs");
    const QString agentLog = tool + QStringLiteral("/agent.log");
    const QString localLog = tool + QStringLiteral("/local.log");
    const QString state = tool + QStringLiteral("/agent-state");
    if (!check(QDir().mkpath(tool + QStringLiteral("/node_modules/agent-desktop"))
        && writeFile(tool + QStringLiteral("/node_modules/agent-desktop/package.json"), "{}")
        && writeFile(tool + QStringLiteral("/configure.mjs"), "success") && writeFile(agentPath, "success"),
        "Panel fixture markers could not be written")) return 2;
    const EnvironmentValue root("AEROSIM_AGENT_DESKTOP_ROOT", temporary.path().toUtf8());
    const EnvironmentValue node("AEROSIM_NODE_EXECUTABLE", QCoreApplication::applicationFilePath().toUtf8());
    const EnvironmentValue pid("AEROSIM_PANEL_TEST_PID", QByteArray::number(QCoreApplication::applicationPid()));

    QMainWindow host;
    // Match MainWindow's expanding workspace above its compact assistant strip.
    auto *central = new QWidget(&host);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(new QWidget(central), 1);
    auto *panel = new aerosim::ParameterAssistantPanel(fixtureSettings, central);
    centralLayout->addWidget(panel);
    host.setCentralWidget(central);
    auto *collapsedOutput = new QDockWidget(QStringLiteral("fixture output"), &host);
    collapsedOutput->setObjectName(QStringLiteral("outputDock"));
    auto *outputTitle = new QLabel(QStringLiteral("collapsed output"), collapsedOutput);
    outputTitle->setFixedHeight(28);
    collapsedOutput->setTitleBarWidget(outputTitle);
    collapsedOutput->setWidget(new QWidget(collapsedOutput));
    collapsedOutput->setFixedHeight(36);
    host.addDockWidget(Qt::BottomDockWidgetArea, collapsedOutput);
    host.resize(1000, 600);
    host.show();
    auto *command = panel->findChild<QLineEdit *>(QStringLiteral("parameterCommandInput"));
    auto *execute = panel->findChild<QPushButton *>(QStringLiteral("executeParameterCommand"));
    auto *settings = panel->findChild<QPushButton *>(QStringLiteral("parameterAssistantExamples"));
    auto *stop = panel->findChild<QPushButton *>(QStringLiteral("cancelParameterCommand"));
    auto *logs = panel->findChild<QPushButton *>(QStringLiteral("parameterAgentLogButton"));
    auto *status = panel->findChild<QLabel *>(QStringLiteral("parameterAssistantStatus"));
    auto *ai = panel->findChild<QAction *>(QStringLiteral("parameterAiModeAction"));
    auto *local = panel->findChild<QAction *>(QStringLiteral("parameterLocalModeAction"));
    auto *agent = panel->findChild<aerosim::ParameterDesktopAgent *>();
    auto *runner = panel->findChild<aerosim::ParameterAssistantRunner *>();
    if (!check(command && execute && settings && stop && logs && status && ai && local && agent && runner,
               "Desktop assistant controls must be discoverable")) return 3;
    int agentStarts = 0;
    int agentFinishes = 0;
    int runnerStarts = 0;
    int runnerFinishes = 0;
    bool agentSuccess = false;
    bool runnerSuccess = false;
    bool busyContract = true;
    QObject::connect(agent, &aerosim::ParameterDesktopAgent::runningChanged, panel,
                     [&](bool running) { if (running) ++agentStarts; });
    QObject::connect(agent, &aerosim::ParameterDesktopAgent::finished, panel,
                     [&](bool success, const QString &) { ++agentFinishes; agentSuccess = success; });
    QObject::connect(runner, &aerosim::ParameterAssistantRunner::runningChanged, panel,
                     [&](bool running) { if (running) ++runnerStarts; });
    QObject::connect(runner, &aerosim::ParameterAssistantRunner::finished, panel,
                     [&](bool success, const QString &, const QString &) { ++runnerFinishes; runnerSuccess = success; });
    const auto busyControls = [&] {
        return command->isReadOnly() && !execute->isEnabled() && !settings->isEnabled()
            && !ai->isEnabled() && !local->isEnabled() && !stop->isHidden() && logs->isEnabled();
    };
    QObject::connect(panel, &aerosim::ParameterAssistantPanel::runningChanged, panel,
                     [&](bool running) { if (running) busyContract = busyContract && busyControls(); });
    QTimer busyPoll;
    QObject::connect(&busyPoll, &QTimer::timeout, panel, [&] {
        if (panel->isRunning()) busyContract = busyContract && busyControls();
    });
    busyPoll.start(5);
    if (!check(ai->isChecked() && !local->isChecked() && ai->text() == text(u"AI\u684c\u9762\u52a9\u624b")
        && execute->text() == text(u"\u6267\u884c\u6307\u4ee4") && !panel->isRunning(),
        "AI desktop mode must be the default, without a plan generation step")) return 4;
    command->setText(fixtureInstruction());
    execute->click();
    auto *dock = host.findChild<QDockWidget *>(QStringLiteral("parameterAgentLogDialog"));
    auto *output = host.findChild<QPlainTextEdit *>(QStringLiteral("parameterAgentLogOutput"));
    auto *clear = host.findChild<QPushButton *>(QStringLiteral("parameterAgentLogClear"));
    auto *copy = host.findChild<QPushButton *>(QStringLiteral("parameterAgentLogCopy"));
    QApplication::processEvents();
    const bool initialDockContract = panel->isRunning() && agent->isRunning() && busyControls() && dock && output && clear && copy
        && !dock->isFloating() && !dock->isHidden() && host.dockWidgetArea(dock) == Qt::BottomDockWidgetArea
        && dock->width() >= host.width() - 4 && dock->width() >= collapsedOutput->width() - 4
        && dock->geometry().top() >= collapsedOutput->geometry().bottom()
        && dock->height() >= 150 && dock->height() <= 230
        && output->isReadOnly() && output->maximumBlockCount() == 3000
        && output->accessibleName() == QStringLiteral("parameterAgentLogOutput")
        && dock->property("agentDesktopPrivate").toBool() && !host.findChild<QDialog *>(QStringLiteral("parameterAiPlanDialog"));
    if (!initialDockContract && dock)
        QTextStream(stderr) << "Dock fixture: host=" << host.width() << 'x' << host.height()
            << " log=" << dock->x() << ',' << dock->y() << ',' << dock->width() << ',' << dock->height()
            << " collapsed=" << collapsedOutput->x() << ',' << collapsedOutput->y() << ',' << collapsedOutput->width() << ',' << collapsedOutput->height()
            << " visible=" << dock->isVisible() << " floating=" << dock->isFloating()
            << " busy=" << panel->isRunning() << " controls=" << busyControls() << '\n';
    if (!check(initialDockContract,
        "Unrestricted desktop instructions must execute directly with a non-obstructing bottom log dock")) return 5;
    logs->click();
    if (!check(waitFor([&] { return output->toPlainText().contains(QStringLiteral("fixture-tool-detail")); })
        && panel->isRunning() && output->toPlainText().contains(QStringLiteral("<b>literal tool output</b>"))
        && !output->toPlainText().contains(fixtureSettings().apiKey)
        && output->toPlainText().contains(QStringLiteral("[redacted]"))
        && QRegularExpression(QStringLiteral("\\[\\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\] \\[INFO\\]")).match(output->toPlainText()).hasMatch(),
        "Tool messages and details must stream as timestamped literal text while the task remains busy")) return 6;
    const QString previousClipboard = QApplication::clipboard()->text();
    copy->click();
    const bool copied = QApplication::clipboard()->text() == output->toPlainText();
    QApplication::clipboard()->setText(previousClipboard);
    if (!check(copied, "Copy all must copy the complete execution log")) return 7;
    if (!check(waitFor([&] { return agentFinishes == 1 && !panel->isRunning(); }) && agentSuccess
        && agentStarts == 1 && runnerStarts == 0 && readFile(state) == QByteArray("verified unrestricted dialog task")
        && status->accessibleName().contains(QStringLiteral("Fixture desktop dialog verified")) && busyContract
        && !command->isReadOnly() && execute->isEnabled() && settings->isEnabled() && stop->isHidden(),
        "Desktop completion must restore controls without using the local parameter runner")) return 8;
    clear->click();
    if (!check(output->toPlainText().isEmpty(), "Clear must remove all displayed log content")) return 9;
    QString manyLines;
    for (int i = 0; i < 3200; ++i) manyLines += QStringLiteral("retained %1\n").arg(i);
    agent->logMessage(QStringLiteral("info"), QStringLiteral("bounded log fixture"), manyLines);
    if (!check(output->document()->blockCount() <= 3000 && output->toPlainText().contains(QStringLiteral("retained 3199")),
        "Long tool output must keep the newest lines within the 3000-line limit")) return 10;
    clear->click();

    if (!check(writeFile(agentPath, "cancel"), "Cancellation marker write failed")) return 11;
    execute->click();
    if (!check(waitFor([&] { return output->toPlainText().contains(QStringLiteral("fixture-tool-detail")); }) && panel->isRunning(),
               "The cancellation fixture must reach a running desktop action")) return 12;
    bool modalFocused = false;
    bool unrelatedKeyPreserved = false;
    bool shortcutAccepted = false;
    bool modalBusy = false;
    QDialog modal(&host);
    QLineEdit modalInput(&modal);
    modalInput.setFocus();
    QObject::connect(agent, &aerosim::ParameterDesktopAgent::finished, &modal,
                     [&modal](bool, const QString &) { modal.reject(); });
    QTimer modalWatchdog;
    modalWatchdog.setSingleShot(true);
    QObject::connect(&modalWatchdog, &QTimer::timeout, &modal, &QDialog::reject);
    modalWatchdog.start(6500);
    QTimer::singleShot(0, &modal, [&] {
        modalFocused = QApplication::activeModalWidget() == &modal;
        QKeyEvent unrelated(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier);
        QApplication::sendEvent(&modalInput, &unrelated);
        unrelatedKeyPreserved = panel->isRunning() && stop->isEnabled();
        QKeyEvent shortcutOverride(QEvent::ShortcutOverride, Qt::Key_F12, Qt::ControlModifier | Qt::ShiftModifier);
        shortcutOverride.ignore();
        QApplication::sendEvent(&modalInput, &shortcutOverride);
        QKeyEvent shortcutPress(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier | Qt::ShiftModifier);
        shortcutPress.ignore();
        QApplication::sendEvent(&modalInput, &shortcutPress);
        shortcutAccepted = shortcutOverride.isAccepted() && shortcutPress.isAccepted();
        modalBusy = panel->isRunning() && !stop->isEnabled() && busyControls();
    });
    modal.exec();
    if (!check(modalFocused && unrelatedKeyPreserved && shortcutAccepted && modalBusy
        && stop->toolTip().contains(QStringLiteral("Ctrl+Shift+F12")),
        "The application stop shortcut must work inside a modal dialog and keep controls busy until exit")) return 13;
    if (!check(waitFor([&] { return agentFinishes == 2 && !panel->isRunning(); }) && !agentSuccess
        && runnerStarts == 0 && readFile(state) == QByteArray("verified unrestricted dialog task") && !dock->isHidden(),
        "Cooperative stopping must preserve prior changes and expose the execution log")) return 14;

    clear->click();
    host.addDockWidget(Qt::TopDockWidgetArea, dock);
    dock->hide();
    if (!check(writeFile(agentPath, "error"), "Failure marker write failed")) return 15;
    execute->click(); dock->hide();
    if (!check(waitFor([&] { return agentFinishes == 3 && !panel->isRunning(); }) && !agentSuccess
        && !dock->isHidden() && output->toPlainText().contains(QStringLiteral("Fixture desktop action failed"))
        && output->toPlainText().contains(QStringLiteral("tool.click fixture failure"))
        && host.dockWidgetArea(dock) == Qt::TopDockWidgetArea
        && runnerStarts == 0 && ai->isChecked() && !local->isChecked(),
        "Agent errors must reveal the log in its manual position without falling back to local rules")) return 16;

    local->trigger();
    command->setText(text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570\u8bbe\u4e3a6"));
    execute->click();
    if (!check(waitFor([&] { return runnerFinishes == 1 && !panel->isRunning(); }) && runnerSuccess
        && runnerStarts == 1 && agentStarts == 3 && local->isChecked() && !ai->isChecked() && busyContract
        && readFile(agentLog) == QByteArray("agent\nagent\nagent\n")
        && QJsonDocument::fromJson(readFile(localLog)).object().value(QStringLiteral("page")).toString() == text(u"\u8ba1\u7b97\u63a7\u5236"),
        "Explicit local mode must preserve the existing parser and verified adapter path")) return 17;

    if (!check(writeFile(tool + QStringLiteral("/configure.mjs"), "cancel"), "Local cancellation marker write failed")) return 18;
    execute->click();
    QKeyEvent localStop(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(command, &localStop);
    if (!check(panel->isRunning() && !stop->isEnabled()
        && waitFor([&] { return runnerFinishes == 2 && !panel->isRunning(); }) && !runnerSuccess && agentStarts == 3,
        "The same stop shortcut must cancel the local runner without invoking AI")) return 19;

    host.addDockWidget(Qt::TopDockWidgetArea, dock);
    dock->show();
    logs->click();
    if (!check(host.dockWidgetArea(dock) == Qt::TopDockWidgetArea,
               "Opening the log must preserve an existing manual dock position")) return 20;
    panel->resetLogLayout();
    if (!check(dock->isHidden() && !dock->isFloating() && host.dockWidgetArea(dock) == Qt::BottomDockWidgetArea,
               "Default layout reset must hide the log and return it to the bottom")) return 20;
    logs->click();
    QApplication::processEvents();
    if (!check(dock->width() >= host.width() - 4 && dock->geometry().top() >= collapsedOutput->geometry().bottom()
        && dock->height() >= 150 && dock->height() <= 230,
        "The reset log layout must show a full separate row below the collapsed output")) return 20;

    QMainWindow restoreHost;
    restoreHost.resize(1000, 600);
    auto *savedDock = new QDockWidget(QStringLiteral("fixture log"), &restoreHost);
    savedDock->setObjectName(QStringLiteral("parameterAgentLogDialog"));
    restoreHost.addDockWidget(Qt::TopDockWidgetArea, savedDock);
    const QByteArray savedLayout = restoreHost.saveState(1);
    restoreHost.removeDockWidget(savedDock);
    delete savedDock;
    if (!check(restoreHost.restoreState(savedLayout, 1), "Lazy log restoration fixture failed")) return 21;
    auto *restorePanel = new aerosim::ParameterAssistantPanel(fixtureSettings, &restoreHost);
    restoreHost.setCentralWidget(restorePanel);
    restorePanel->findChild<QPushButton *>(QStringLiteral("parameterAgentLogButton"))->click();
    auto *restoredDock = restoreHost.findChild<QDockWidget *>(QStringLiteral("parameterAgentLogDialog"));
    if (!check(restoredDock && restoreHost.dockWidgetArea(restoredDock) == Qt::TopDockWidgetArea,
               "A lazily created log dock must restore its saved position")) return 22;

    aerosim::ParameterAssistantPanel standalone(fixtureSettings);
    standalone.show();
    standalone.findChild<QPushButton *>(QStringLiteral("parameterAgentLogButton"))->click();
    auto *dialog = standalone.findChild<QDialog *>(QStringLiteral("parameterAgentLogDialog"));
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    if (!check(dialog && dialog->isVisible() && !dialog->isModal() && dialog->width() <= available.width()
        && dialog->height() <= available.height(), "Standalone assistant logs must use a screen-sized nonmodal dialog")) return 23;
    dialog->close();
    QTextStream(stdout) << "Desktop assistant direct execution, live dock logs, cancellation and local mode passed\n";
    return 0;
}
