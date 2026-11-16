#include "aerosim/ui/ParameterAiSettings.h"
#include "aerosim/ui/ParameterAiSettingsDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
bool check(bool passed, const char *message)
{
    if (!passed) QTextStream(stderr) << message << '\n';
    return passed;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "Temporary settings directory failed")) return 1;
    const QSettings::Format previousFormat = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings saved(QSettings::defaultFormat(), QSettings::UserScope,
        QStringLiteral("AeroSimWorkbench"), QStringLiteral("ParameterAssistant"));

    // Every save below is invalid and stops before any credential read, write or deletion.
    // loadParameterAiSettings/hasParameterAiSettings are intentionally not called here.
    QString error;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("http://remote.invalid/v1"), QStringLiteral("model"), QStringLiteral("sample")}, &error)
               && !error.isEmpty(), "Remote HTTP endpoint must be rejected")) return 4;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://name:password@example.invalid/v1"), QStringLiteral("model"), QStringLiteral("sample")}, &error),
               "Endpoint credentials must be rejected")) return 5;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://example.invalid/v1?token=sample"), QStringLiteral("model"), QStringLiteral("sample")}, &error),
               "Endpoint query secrets must be rejected")) return 6;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://example.invalid/v1"), QString(), QStringLiteral("sample")}, &error),
               "Empty model must be rejected")) return 7;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("http://localhost:4567/v1"), QStringLiteral("model"), QStringLiteral("one\ntwo")}, &error),
               "Multiline API key must be rejected even for a valid local endpoint")) return 8;
    const QString keyError = error;
    if (!check(!keyError.isEmpty(), "Invalid key validation must provide a message")) return 14;
    const auto rejectedAtKey = [&](const QString &endpoint, const QString &model) {
        return !aerosim::saveParameterAiSettings({endpoint, model, QStringLiteral("invalid key")}, &error) && error == keyError;
    };
    if (!check(rejectedAtKey(QStringLiteral("https://example.invalid/v1"), QString(128, QLatin1Char('m'))),
               "A 128-character model must reach key validation")) return 15;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://example.invalid/v1"), QString(129, QLatin1Char('m')), QStringLiteral("invalid key")}, &error)
               && error != keyError, "A 129-character model must be rejected before key validation")) return 16;
    const QString modelError = error;
    if (!check(rejectedAtKey(QStringLiteral("https://example.invalid/v1/responses///"), QStringLiteral("Org/model_v2.1:fast-1")),
               "Response endpoints and the complete supported model character set must be accepted")) return 17;
    const QStringList invalidModels{QStringLiteral("-model"), QStringLiteral("model name"), QStringLiteral("model@provider"),
                                    QStringLiteral("model\tname"), QString(QChar(0x6a21))};
    for (const QString &model : invalidModels)
        if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://example.invalid/v1"), model, QStringLiteral("invalid key")}, &error)
                   && error == modelError, "Invalid model names must not reach credential storage")) return 18;
    const QStringList invalidKeys{QStringLiteral("one two"), QStringLiteral("one\ttwo"), QStringLiteral("one\rtwo"),
                                  QStringLiteral("one\ntwo"), QStringLiteral("one") + QChar(0) + QStringLiteral("two"),
                                  QStringLiteral("one") + QChar(0x7f) + QStringLiteral("two"), QString(QChar(0x4e2d)),
                                  QString(8193, QLatin1Char('k'))};
    for (const QString &invalidKey : invalidKeys)
        if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://example.invalid/v1"), QStringLiteral("model"), invalidKey}, &error)
                   && error == keyError, "Whitespace, controls, non-ASCII and oversized keys must be rejected")) return 19;
    if (!check(rejectedAtKey(QStringLiteral("http://127.0.0.1:4567/v1"), QStringLiteral("model"))
               && rejectedAtKey(QStringLiteral("http://[::1]:4567/v1/responses"), QStringLiteral("model")),
               "IPv4 and IPv6 loopback HTTP endpoints must reach key validation")) return 20;
    const QStringList invalidEndpoints{QStringLiteral("https://example.invalid/v1/chat/completions"),
        QStringLiteral("https://example.invalid/V1/CHAT/COMPLETIONS///"),
        QStringLiteral("https://example.invalid/v1#fragment"), QStringLiteral("http://localhost.example.invalid/v1"),
        QStringLiteral("https://example.invalid/") + QString(2048, QLatin1Char('p'))};
    for (const QString &endpoint : invalidEndpoints)
        if (!check(!aerosim::saveParameterAiSettings({endpoint, QStringLiteral("model"), QStringLiteral("invalid key")}, &error)
                   && error != keyError, "Unsupported endpoint formats must be rejected before credential access")) return 21;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://example.invalid/path-secret/v1"), QStringLiteral("model"), QStringLiteral("path-secret")}, &error)
               && error != keyError, "A key embedded in a saved endpoint must be rejected")) return 22;
    if (!check(!aerosim::saveParameterAiSettings({QStringLiteral("https://example.invalid/path%2Dsecret/v1"), QStringLiteral("model"), QStringLiteral("path-secret")}, &error)
               && error != keyError, "A percent-encoded key embedded in an endpoint must be rejected")) return 23;

    // The dialog fixture uses a synthetic key and never clicks a valid save operation.
    aerosim::ParameterAiSettingsDialog dialog({QStringLiteral("https://example.invalid/v1"), QStringLiteral("model"), QStringLiteral("synthetic-key")});
    auto *key = dialog.findChild<QLineEdit *>(QStringLiteral("parameterAiApiKey"));
    auto *reveal = dialog.findChild<QCheckBox *>(QStringLiteral("parameterAiShowKey"));
    auto *clear = dialog.findChild<QCheckBox *>(QStringLiteral("parameterAiClearKey"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("parameterAiButtons"));
    auto *status = dialog.findChild<QLabel *>(QStringLiteral("parameterAiStatus"));
    if (!check(key && reveal && clear && buttons && status && key->echoMode() == QLineEdit::Password,
               "API key must be hidden by default")) return 9;
    reveal->setChecked(true);
    if (!check(key->echoMode() == QLineEdit::Normal, "Explicit key reveal did not work")) return 10;
    clear->setChecked(true);
    if (!check(dialog.settings().apiKey.isEmpty() && !key->isEnabled() && key->echoMode() == QLineEdit::Password,
               "Explicit clear must remove the returned key and hide/disable the field")) return 11;
    clear->setChecked(false);
    key->clear();
    buttons->button(QDialogButtonBox::Save)->click();
    if (!check(!status->text().isEmpty() && dialog.result() != QDialog::Accepted,
               "Clearing a key without the explicit clear option must not save")) return 12;
    for (const QString &name : saved.allKeys())
        if (!check(!name.contains(QStringLiteral("key"), Qt::CaseInsensitive), "QSettings must never contain an API key entry")) return 13;
    QSettings::setDefaultFormat(previousFormat);
    QTextStream(stdout) << "AI settings endpoint/model/key validation and explicit credential UI passed; no credentials were accessed\n";
    return 0;
}
