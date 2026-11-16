#include "aerosim/ui/ParameterAssistantPanel.h"
#include "aerosim/ui/ParameterAssistantRunner.h"
#include "aerosim/ui/ParameterCommandParser.h"
#include "aerosim/ui/ParameterDesktopAgent.h"
#include "aerosim/ui/ParameterAiSettingsDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDockWidget>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextCursor>
#include <QVBoxLayout>
#include <cstddef>
#include <utility>

namespace aerosim {
namespace {
template <std::size_t N> QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}
}
ParameterAssistantPanel::ParameterAssistantPanel(QWidget *parent)
    : ParameterAssistantPanel(loadParameterAiSettings, parent)
{
}

ParameterAssistantPanel::ParameterAssistantPanel(std::function<ParameterAiSettings()> settingsLoader, QWidget *parent)
    : QWidget(parent), settingsLoader_(std::move(settingsLoader))
{
    if (!settingsLoader_) settingsLoader_ = loadParameterAiSettings;
    setObjectName(QStringLiteral("parameterAssistant"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    setStyleSheet(QStringLiteral(
        "QWidget#parameterAssistant { border-top: 1px solid #cbd8e8; }"
        "QLabel#parameterAssistantTitle { font-weight: 600; color: #24588c; }"
        "QPushButton#executeParameterCommand { background: #245f9e; color: white; padding: 6px 14px; border-radius: 4px; }"
        "QPushButton#executeParameterCommand:disabled { background: #9eb2c9; }"));
    runner_ = new ParameterAssistantRunner(this);
    agent_ = new ParameterDesktopAgent(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 8, 24, 8);
    layout->setSpacing(5);
    auto *input = new QHBoxLayout;
    auto *title = new QLabel(text(u"AI\u684c\u9762\u52a9\u624b"), this);
    title->setObjectName(QStringLiteral("parameterAssistantTitle"));
    input->addWidget(title);
    command_ = new QLineEdit(this);
    command_->setObjectName(QStringLiteral("parameterCommandInput"));
    command_->setAccessibleName(text(u"\u684c\u9762\u64cd\u4f5c\u6307\u4ee4"));
    command_->setMaxLength(8192);
    command_->setMinimumWidth(180);
    command_->setMinimumHeight(32);
    title->setBuddy(command_);
    input->addWidget(command_, 1);
    execute_ = new QPushButton(text(u"\u6267\u884c\u6307\u4ee4"), this);
    execute_->setObjectName(QStringLiteral("executeParameterCommand"));
    input->addWidget(execute_);
    settings_ = new QPushButton(text(u"\u52a9\u624b\u8bbe\u7f6e"), this);
    settings_->setObjectName(QStringLiteral("parameterAssistantExamples"));
    auto *menu = new QMenu(settings_);
    settings_->setMenu(menu);
    input->addWidget(settings_);
    auto *settingsAction = menu->addAction(text(u"AI \u8bbe\u7f6e\u2026"));
    settingsAction->setObjectName(QStringLiteral("parameterAiSettingsAction"));
    connect(settingsAction, &QAction::triggered, this, &ParameterAssistantPanel::openAiSettings);
    menu->addSeparator();
    auto *modeGroup = new QActionGroup(this);
    aiModeAction_ = menu->addAction(text(u"AI\u684c\u9762\u52a9\u624b"));
    aiModeAction_->setObjectName(QStringLiteral("parameterAiModeAction"));
    localModeAction_ = menu->addAction(text(u"\u672c\u5730\u89c4\u5219\u6a21\u5f0f"));
    localModeAction_->setObjectName(QStringLiteral("parameterLocalModeAction"));
    for (auto *action : {aiModeAction_, localModeAction_}) {
        action->setCheckable(true);
        modeGroup->addAction(action);
    }
    aiModeAction_->setChecked(true);
    connect(aiModeAction_, &QAction::triggered, this, [this] { setAiMode(true); });
    connect(localModeAction_, &QAction::triggered, this, [this] { setAiMode(false); });
    menu->addSeparator();
    const auto example = [this, menu](const QString &label, const QString &instruction) {
        auto *action = menu->addAction(label);
        action->setToolTip(instruction);
        connect(action, &QAction::triggered, this, [this, instruction] {
            if (isRunning()) return;
            command_->setText(instruction);
            command_->setFocus();
        });
    };
    example(text(u"\u8ba1\u7b97\u63a7\u5236\u793a\u4f8b"), text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u6570\u8bbe\u4e3a12000\uff0c\u8fdb\u7a0b\u65706\uff0c\u542f\u7528GPU"));
    example(text(u"\u7f51\u683c\u8fd0\u52a8\u793a\u4f8b"), text(u"\u6253\u5f00\u7f51\u683c\u8fd0\u52a8\u8bbe\u7f6e\u7a97\u53e3\uff0c\u8bf4\u660e\u5f53\u524d\u53ef\u914d\u7f6e\u7684\u9009\u9879"));
    auto *help = menu->addAction(text(u"\u4f7f\u7528\u8bf4\u660e"));
    help->setObjectName(QStringLiteral("parameterAssistantHelpAction"));
    connect(help, &QAction::triggered, this, [this] {
        QMessageBox dialog(QMessageBox::Information, text(u"AI\u684c\u9762\u52a9\u624b \u00b7 \u4f7f\u7528\u8bf4\u660e"), text(
            u"AI\u89c2\u5bdf\u5f53\u524d\u5de5\u4f5c\u53f0\u754c\u9762\uff0c\u6839\u636e\u6307\u4ee4\u64cd\u4f5c\u5404\u4e2a\u9875\u9762\u548c\u5f39\u7a97\uff0c\u5e76\u6838\u5bf9\u7ed3\u679c\uff1b\u4e0d\u6309\u53c2\u6570\u767d\u540d\u5355\u9650\u5236\u3002\n\n"
            u"\u8bf7\u8bf4\u660e\u8981\u64cd\u4f5c\u7684\u754c\u9762\u548c\u671f\u671b\u7ed3\u679c\u3002\u6267\u884c\u65f6\u53ef\u6253\u5f00\u201c\u6267\u884c\u65e5\u5fd7\u201d\u67e5\u770b\u8fdb\u5ea6\u3001\u5de5\u5177\u8c03\u7528\u548c\u7ed3\u679c\uff0c\u6216\u70b9\u51fb\u201c\u505c\u6b62\u201d\u3002\u5f39\u7a97\u4e2d\u4e5f\u53ef\u6309 Ctrl+Shift+F12 \u505c\u6b62\u3002\u5df2\u6267\u884c\u7684\u64cd\u4f5c\u4f1a\u4fdd\u7559\u3002\n\n"
            u"\u672c\u5730\u89c4\u5219\u6a21\u5f0f\u4ecd\u652f\u6301\u8ba1\u7b97\u63a7\u5236\u548c\u53c2\u8003\u91cf\uff0c\u65e0\u9700\u8c03\u7528AI\uff0c\u6bcf\u6b21\u914d\u7f6e\u4e00\u4e2a\u9875\u9762\u3002"), QMessageBox::Ok, this);
        dialog.setTextFormat(Qt::PlainText);
        dialog.exec();
    });
    cancel_ = new QPushButton(text(u"\u505c\u6b62"), this);
    cancel_->setObjectName(QStringLiteral("cancelParameterCommand"));
    cancel_->setToolTip(text(u"\u505c\u6b62\u5f53\u524d\u4efb\u52a1\uff08Ctrl+Shift+F12\uff0c\u5f39\u7a97\u4e2d\u4ecd\u53ef\u4f7f\u7528\uff09"));
    cancel_->hide();
    input->addWidget(cancel_);
    layout->addLayout(input);
    auto *feedback = new QHBoxLayout;
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("parameterAssistantStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setFixedHeight(status_->fontMetrics().height() + 2);
    status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    feedback->addWidget(status_, 1);
    log_ = new QPushButton(text(u"\u6267\u884c\u65e5\u5fd7"), this);
    log_->setObjectName(QStringLiteral("parameterAgentLogButton"));
    log_->setFlat(true);
    feedback->addWidget(log_);
    details_ = new QPushButton(text(u"\u67e5\u770b\u7ed3\u679c"), this);
    details_->setObjectName(QStringLiteral("parameterCommandDetails"));
    details_->setFlat(true);
    details_->hide();
    feedback->addWidget(details_);
    layout->addLayout(feedback);
    connect(command_, &QLineEdit::returnPressed, this, &ParameterAssistantPanel::execute);
    connect(execute_, &QPushButton::clicked, this, &ParameterAssistantPanel::execute);
    connect(log_, &QPushButton::clicked, this, &ParameterAssistantPanel::showExecutionLog);
    connect(cancel_, &QPushButton::clicked, this, &ParameterAssistantPanel::stopExecution);
    connect(runner_, &ParameterAssistantRunner::runningChanged, this, [this] { refreshBusyState(); });
    connect(agent_, &ParameterDesktopAgent::runningChanged, this, [this] { refreshBusyState(); });
    connect(agent_, &ParameterDesktopAgent::logMessage, this,
            [this](const QString &level, const QString &message, const QString &detail) {
        appendLog(level, message, detail);
        if (!message.isEmpty()) setStatus(redact(message), level.compare(QStringLiteral("error"), Qt::CaseInsensitive) == 0);
    });
    connect(agent_, &ParameterDesktopAgent::finished, this, [this](bool success, const QString &message) {
        appendLog(success ? QStringLiteral("info") : QStringLiteral("error"), message);
        setStatus(redact(message), !success);
        activeKey_.clear();
        if (!success) showExecutionLog();
    });
    connect(runner_, &ParameterAssistantRunner::finished, this,
            [this](bool success, const QString &message, const QString &details) {
        lastDetails_ = details;
        details_->setVisible(!lastDetails_.isEmpty());
        appendLog(success ? QStringLiteral("info") : QStringLiteral("error"), message, details);
        setStatus(message, !success);
        if (!success) showExecutionLog();
    });
    connect(details_, &QPushButton::clicked, this, [this] {
        QMessageBox dialog(QMessageBox::Information, text(u"\u53c2\u6570\u52a9\u624b\u7ed3\u679c"), statusText_, QMessageBox::Ok, this);
        dialog.setTextFormat(Qt::PlainText);
        dialog.setDetailedText(lastDetails_);
        dialog.exec();
    });
    setAiMode(true);
    qApp->installEventFilter(this);
}

bool ParameterAssistantPanel::isRunning() const
{
    return (runner_ && runner_->isRunning()) || (agent_ && agent_->isRunning());
}

void ParameterAssistantPanel::stopExecution()
{
    if (!isRunning() || stopRequested_) return;
    stopRequested_ = true;
    cancel_->setEnabled(false);
    appendLog(QStringLiteral("info"), text(u"\u5df2\u8bf7\u6c42\u505c\u6b62\uff0c\u6b63\u5728\u7b49\u5f85\u5de5\u5177\u7ed3\u675f\u3002"));
    setStatus(text(u"\u6b63\u5728\u505c\u6b62\uff1b\u5df2\u6267\u884c\u7684\u64cd\u4f5c\u4f1a\u4fdd\u7559\u3002"));
    if (agent_->isRunning()) agent_->cancel();
    else runner_->cancel();
}

bool ParameterAssistantPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (isRunning() && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_F12 && key->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) stopExecution();
            event->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ParameterAssistantPanel::refreshBusyState()
{
    const bool busy = isRunning();
    command_->setReadOnly(busy);
    execute_->setEnabled(!busy);
    settings_->setEnabled(!busy);
    aiModeAction_->setEnabled(!busy);
    localModeAction_->setEnabled(!busy);
    cancel_->setVisible(busy);
    cancel_->setEnabled(busy && !stopRequested_);
    details_->setEnabled(!busy);
    if (lastBusy_ != busy) { lastBusy_ = busy; emit runningChanged(busy); }
}

void ParameterAssistantPanel::setAiMode(bool enabled)
{
    if (isRunning()) return;
    aiMode_ = enabled;
    aiModeAction_->setChecked(enabled);
    localModeAction_->setChecked(!enabled);
    execute_->setText(text(u"\u6267\u884c\u6307\u4ee4"));
    command_->setPlaceholderText(enabled ? text(u"\u63cf\u8ff0\u8981\u64cd\u4f5c\u7684\u754c\u9762\u548c\u7ed3\u679c\uff0c\u4f8b\u5982\uff1a\u6253\u5f00\u7f51\u683c\u8fd0\u52a8\u8bbe\u7f6e\u5e76\u67e5\u770b\u5f53\u524d\u9009\u9879")
        : text(u"\u4f8b\u5982\uff1a\u9a6c\u8d6b\u65700.8\uff0c\u653b\u89d25\u5ea6\uff1b\u540c\u4e00\u9875\u7684\u591a\u4e2a\u53c2\u6570\u7528\u9017\u53f7\u5206\u9694"));
    setStatus(enabled ? text(u"AI\u684c\u9762\u52a9\u624b \u00b7 \u89c2\u5bdf\u5f53\u524d\u5de5\u4f5c\u53f0\uff0c\u6309\u6307\u4ee4\u64cd\u4f5c\u754c\u9762\u5e76\u6838\u5bf9\u7ed3\u679c\u3002")
        : text(u"\u672c\u5730\u89c4\u5219\u6a21\u5f0f \u00b7 \u652f\u6301\u8ba1\u7b97\u63a7\u5236\u548c\u53c2\u8003\u91cf\uff0c\u6267\u884c\u540e\u56de\u8bfb\u9a8c\u8bc1\u3002"));
}

void ParameterAssistantPanel::openAiSettings()
{
    if (isRunning()) return;
    ParameterAiSettingsDialog dialog(settingsLoader_(), this);
    dialog.setProperty("agentDesktopPrivate", true);
    dialog.setAccessibleName(QStringLiteral("parameterAiSettingsDialog"));
    if (dialog.exec() == QDialog::Accepted)
        setStatus(hasParameterAiSettings(settingsLoader_()) ? text(u"AI\u914d\u7f6e\u5df2\u4fdd\u5b58\uff0c\u53ef\u4ee5\u6267\u884c\u684c\u9762\u6307\u4ee4\u3002") : text(u"AI\u914d\u7f6e\u5df2\u4fdd\u5b58\uff0c\u8bf7\u8865\u5145API Key\u540e\u4f7f\u7528\u3002"));
}

void ParameterAssistantPanel::execute()
{
    if (isRunning()) return;
    if (command_->text().trimmed().isEmpty()) {
        setStatus(text(u"\u8bf7\u5148\u8f93\u5165\u8981\u6267\u884c\u7684\u6307\u4ee4\u3002"), true);
        return;
    }
    details_->hide(); lastDetails_.clear();
    stopRequested_ = false;
    if (aiMode_) {
        auto settings = settingsLoader_();
        if (!hasParameterAiSettings(settings)) {
            openAiSettings();
            settings = settingsLoader_();
            if (!hasParameterAiSettings(settings)) return;
        }
        activeKey_ = settings.apiKey;
        appendLog(QStringLiteral("task"), text(u"\u5f00\u59cbAI\u684c\u9762\u4efb\u52a1\uff08Ctrl+Shift+F12 \u505c\u6b62\uff09"), command_->text());
        if (logDock_) showExecutionLog();
        setStatus(text(u"AI\u6b63\u5728\u89c2\u5bdf\u5de5\u4f5c\u53f0\u5e76\u6267\u884c\u6307\u4ee4\u2026"));
        agent_->start(command_->text(), settings);
        return;
    }
    const auto result = parseParameterCommand(command_->text());
    if (!result.ok) { setStatus(result.error, true); return; }
    appendLog(QStringLiteral("task"), text(u"\u5f00\u59cb\u672c\u5730\u89c4\u5219\u4efb\u52a1\uff08Ctrl+Shift+F12 \u505c\u6b62\uff09"), command_->text());
    setStatus(text(u"\u6b63\u5728\u914d\u7f6e\u5e76\u56de\u8bfb\u9a8c\u8bc1\uff0c\u8bf7\u6682\u52ff\u64cd\u4f5c\u53c2\u6570\u754c\u9762\u2026"));
    runner_->start(result.preset);
}

void ParameterAssistantPanel::ensureLogWindow()
{
    if (logWindow_ && logOutput_) return;
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 8, 10, 8);
    logOutput_ = new QPlainTextEdit(content);
    logOutput_->setObjectName(QStringLiteral("parameterAgentLogOutput"));
    logOutput_->setAccessibleName(QStringLiteral("parameterAgentLogOutput"));
    logOutput_->setProperty("agentDesktopPrivate", true);
    logOutput_->setReadOnly(true);
    logOutput_->setUndoRedoEnabled(false);
    logOutput_->setMaximumBlockCount(3000);
    logOutput_->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(logOutput_, 1);
    auto *actions = new QHBoxLayout;
    auto *clear = new QPushButton(text(u"\u6e05\u7a7a"), content);
    clear->setObjectName(QStringLiteral("parameterAgentLogClear"));
    auto *copy = new QPushButton(text(u"\u590d\u5236\u5168\u90e8"), content);
    copy->setObjectName(QStringLiteral("parameterAgentLogCopy"));
    actions->addStretch(); actions->addWidget(clear); actions->addWidget(copy);
    layout->addLayout(actions);
    connect(clear, &QPushButton::clicked, this, [this] { if (logOutput_) logOutput_->clear(); });
    connect(copy, &QPushButton::clicked, this, [this] {
        if (logOutput_) QApplication::clipboard()->setText(logOutput_->toPlainText());
    });
    bool restored = false;
    if (auto *host = qobject_cast<QMainWindow *>(window())) {
        auto *dock = new QDockWidget(text(u"AI\u6267\u884c\u65e5\u5fd7"), host);
        dock->setObjectName(QStringLiteral("parameterAgentLogDialog"));
        dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        dock->setWidget(content);
        restored = host->restoreDockWidget(dock);
        logWindow_ = dock;
        logDock_ = dock;
        if (!restored) placeLogDockAtBottom();
    } else {
        auto *dialog = new QDialog(this);
        dialog->setWindowTitle(text(u"AI\u6267\u884c\u65e5\u5fd7"));
        dialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        dialog->setModal(false);
        auto *dialogLayout = new QVBoxLayout(dialog);
        dialogLayout->setContentsMargins(0, 0, 0, 0);
        dialogLayout->addWidget(content);
        const QRect available = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->availableGeometry() : QRect(0, 0, 1024, 768);
        dialog->resize(qMin(900, qMax(300, available.width() - 80)), qMin(520, qMax(180, available.height() - 100)));
        logWindow_ = dialog;
    }
    logWindow_->setObjectName(QStringLiteral("parameterAgentLogDialog"));
    logWindow_->setAccessibleName(QStringLiteral("parameterAgentLogDialog"));
    logWindow_->setProperty("agentDesktopPrivate", true);
    if (!restored) logWindow_->hide();
}

void ParameterAssistantPanel::showExecutionLog()
{
    ensureLogWindow();
    logWindow_->show();
    if (logDock_ && logDefaultSizePending_) {
        if (auto *host = qobject_cast<QMainWindow *>(logDock_->parentWidget())) {
            // Hidden docks do not receive reliable resizeDocks geometry. Size the separate
            // row only after showing it, without changing subsequent manual placement.
            if (host->layout()) host->layout()->activate();
            host->resizeDocks({logDock_}, {qBound(160, host->height() / 4, 220)}, Qt::Vertical);
        }
        logDefaultSizePending_ = false;
    }
    if (!logDock_) { logWindow_->raise(); logWindow_->activateWindow(); }
}

void ParameterAssistantPanel::placeLogDockAtBottom()
{
    if (!logDock_) return;
    if (auto *host = qobject_cast<QMainWindow *>(logDock_->parentWidget())) {
        logDock_->setFloating(false);
        host->removeDockWidget(logDock_);
        auto *output = host->findChild<QDockWidget *>(QStringLiteral("outputDock"));
        if (output && !output->isFloating() && host->dockWidgetArea(output) == Qt::BottomDockWidgetArea)
            host->splitDockWidget(output, logDock_, Qt::Vertical);
        else
            host->addDockWidget(Qt::BottomDockWidgetArea, logDock_);
        logDefaultSizePending_ = true;
    }
}

void ParameterAssistantPanel::resetLogLayout()
{
    if (!logWindow_) return;
    placeLogDockAtBottom();
    logWindow_->hide();
}

QString ParameterAssistantPanel::redact(const QString &message) const
{
    QString safe = message;
    if (!activeKey_.isEmpty()) safe.replace(activeKey_, QStringLiteral("[redacted]"));
    return safe;
}

void ParameterAssistantPanel::appendLog(const QString &level, const QString &message, const QString &detail)
{
    ensureLogWindow();
    QString safeLevel = redact(level).left(24).toUpper();
    safeLevel.remove(QLatin1Char('\n')); safeLevel.remove(QLatin1Char('\r'));
    const QString prefix = QStringLiteral("[%1] [%2] ").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), safeLevel);
    const QString safeMessage = redact(message).left(8192);
    const QString safeDetail = redact(detail).left(65536);
    QString entry = prefix + safeMessage;
    if (!safeDetail.isEmpty()) entry += QLatin1Char('\n') + prefix + safeDetail;
    // Model/tool output is appended as literal text, never as HTML.
    auto *scroll = logOutput_->verticalScrollBar();
    const bool follow = scroll->value() >= scroll->maximum() - 2;
    logOutput_->appendPlainText(entry);
    if (follow) { logOutput_->moveCursor(QTextCursor::End); logOutput_->ensureCursorVisible(); }
    if (safeLevel == QStringLiteral("ERROR") || safeLevel == QStringLiteral("FATAL")) showExecutionLog();
}

void ParameterAssistantPanel::setStatus(const QString &message, bool error)
{
    statusText_ = message;
    status_->setAccessibleName(message);
    status_->setText(status_->fontMetrics().elidedText(message, Qt::ElideRight, qMax(80, status_->width())));
    status_->setToolTip(message);
    status_->setStyleSheet(error ? QStringLiteral("color: #b03b30;") : QStringLiteral("color: #54718c;"));
}

void ParameterAssistantPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    status_->setText(status_->fontMetrics().elidedText(statusText_, Qt::ElideRight, qMax(80, status_->width())));
}
} // namespace aerosim
