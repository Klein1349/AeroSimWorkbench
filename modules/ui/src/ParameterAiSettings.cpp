#include "aerosim/ui/ParameterAiSettings.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <cstddef>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#else
#include <QMutex>
#include <QMutexLocker>
#endif

namespace aerosim {
namespace {
template <std::size_t N>
QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

const char kOrganization[] = "AeroSimWorkbench";
const char kApplication[] = "ParameterAssistant";
const char kGroup[] = "ParameterAssistantAi";
const char kDefaultEndpoint[] = "https://api.site/v1";
const char kDefaultModel[] = "gpt-6-sol";

bool validEndpoint(const QString &endpoint)
{
    if (endpoint.isEmpty() || endpoint.size() > 2048) return false;
    const QUrl url(endpoint, QUrl::StrictMode);
    QString path = url.path(QUrl::FullyEncoded);
    while (path.endsWith(QLatin1Char('/'))) path.chop(1);
    if (path.endsWith(QStringLiteral("/chat/completions"), Qt::CaseInsensitive)) return false;
    const bool localHttp = url.scheme() == QStringLiteral("http")
        && (url.host() == QStringLiteral("localhost") || url.host() == QStringLiteral("127.0.0.1")
            || url.host() == QStringLiteral("::1"));
    return url.isValid() && (url.scheme() == QStringLiteral("https") || localHttp) && !url.host().isEmpty()
        && url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment();
}

bool validModel(const QString &model)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:/-]{0,127}$"));
    return pattern.match(model).hasMatch();
}

bool validApiKey(const QString &key)
{
    if (key.isEmpty() || key.size() > 8192) return false;
    for (const QChar character : key)
        if (character.unicode() < 0x21 || character.unicode() > 0x7e) return false;
    return true;
}

#ifdef Q_OS_WIN
const wchar_t kCredentialTarget[] = L"AeroSimWorkbench/ParameterAssistant/RightAPI";

QString readCredential()
{
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(kCredentialTarget, CRED_TYPE_GENERIC, 0, &credential)) return {};
    QString key;
    if (credential->CredentialBlob && credential->CredentialBlobSize <= CRED_MAX_CREDENTIAL_BLOB_SIZE)
        key = QString::fromUtf8(reinterpret_cast<const char *>(credential->CredentialBlob), int(credential->CredentialBlobSize));
    if (credential->CredentialBlob) SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
    CredFree(credential);
    return key;
}

bool writeCredential(const QString &key, QString *error)
{
    if (key.isEmpty()) {
        if (CredDeleteW(kCredentialTarget, CRED_TYPE_GENERIC, 0)) return true;
        const DWORD errorCode = GetLastError();
        if (errorCode == ERROR_NOT_FOUND) return true;
        if (error) *error = text(u"\u65e0\u6cd5\u79fb\u9664\u5df2\u4fdd\u5b58\u5bc6\u94a5\uff08Windows \u9519\u8bef %1\uff09\u3002").arg(errorCode);
        return false;
    }
    QByteArray secret = key.toUtf8();
    if (secret.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        if (error) *error = text(u"\u5bc6\u94a5\u957f\u5ea6\u8d85\u51fa Windows \u51ed\u636e\u9650\u5236\u3002");
        SecureZeroMemory(secret.data(), SIZE_T(secret.size()));
        return false;
    }
    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(kCredentialTarget);
    credential.CredentialBlobSize = DWORD(secret.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(secret.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"AeroSimWorkbench");
    const bool success = CredWriteW(&credential, 0) != FALSE;
    const DWORD errorCode = success ? ERROR_SUCCESS : GetLastError();
    SecureZeroMemory(secret.data(), SIZE_T(secret.size()));
    if (!success && error)
        *error = text(u"\u65e0\u6cd5\u5b89\u5168\u4fdd\u5b58\u5bc6\u94a5\uff08Windows \u9519\u8bef %1\uff09\u3002").arg(errorCode);
    return success;
}
#else
QMutex sessionCredentialMutex;
QString sessionCredential;

QString readCredential()
{
    const QMutexLocker lock(&sessionCredentialMutex);
    return sessionCredential;
}

bool writeCredential(const QString &key, QString *)
{
    const QMutexLocker lock(&sessionCredentialMutex);
    sessionCredential = key;
    return true;
}
#endif

QString fallback(const QString &saved, const char *environment, const QString &defaultValue = QString())
{
    if (!saved.isEmpty()) return saved;
    const QString value = qEnvironmentVariable(environment).trimmed();
    return value.isEmpty() ? defaultValue : value;
}
} // namespace

ParameterAiSettings loadParameterAiSettings()
{
    QSettings saved(QSettings::defaultFormat(), QSettings::UserScope,
        QString::fromLatin1(kOrganization), QString::fromLatin1(kApplication));
    saved.beginGroup(QString::fromLatin1(kGroup));
    ParameterAiSettings settings;
    settings.endpoint = fallback(saved.value(QStringLiteral("endpoint")).toString().trimmed(), "AEROSIM_AI_BASE_URL", QString::fromLatin1(kDefaultEndpoint));
    settings.model = fallback(saved.value(QStringLiteral("model")).toString().trimmed(), "AEROSIM_AI_MODEL", QString::fromLatin1(kDefaultModel));
    settings.apiKey = fallback(readCredential(), "AEROSIM_AI_API_KEY");
    return settings;
}

bool saveParameterAiSettings(const ParameterAiSettings &settings, QString *error)
{
    if (error) error->clear();
    const QString endpoint = settings.endpoint.trimmed();
    const QString model = settings.model.trimmed();
    const QString key = settings.apiKey.trimmed();
    if (!validEndpoint(endpoint)) {
        if (error) *error = text(u"\u8bf7\u586b\u5199\u6709\u6548\u7684 HTTPS \u63a5\u53e3\u5730\u5740\uff08\u4ec5\u672c\u673a\u5730\u5740\u53ef\u7528 HTTP\uff09\uff0c\u4e0d\u5305\u542b\u7528\u6237\u540d\u3001\u67e5\u8be2\u53c2\u6570\u6216\u7247\u6bb5\u3002");
        return false;
    }
    if (!validModel(model)) {
        if (error) *error = text(u"\u8bf7\u586b\u5199\u6709\u6548\u7684\u6a21\u578b\u540d\u79f0\u3002");
        return false;
    }
    if (!key.isEmpty() && !validApiKey(key)) {
        if (error) *error = text(u"\u5bc6\u94a5\u4ec5\u80fd\u5305\u542b\u53ef\u6253\u5370\u7684 ASCII \u5b57\u7b26\uff0c\u4e0d\u80fd\u5305\u542b\u7a7a\u683c\u3001\u6362\u884c\u6216\u63a7\u5236\u5b57\u7b26\u3002");
        return false;
    }
    if (!key.isEmpty() && (endpoint.contains(key) || QUrl::fromPercentEncoding(endpoint.toUtf8()).contains(key))) {
        if (error) *error = text(u"\u63a5\u53e3\u5730\u5740\u4e2d\u4e0d\u80fd\u5305\u542b API Key\u3002");
        return false;
    }
    if (!writeCredential(key, error)) return false;
    QSettings saved(QSettings::defaultFormat(), QSettings::UserScope,
        QString::fromLatin1(kOrganization), QString::fromLatin1(kApplication));
    saved.beginGroup(QString::fromLatin1(kGroup));
    saved.setValue(QStringLiteral("endpoint"), endpoint);
    saved.setValue(QStringLiteral("model"), model);
    // The credential is deliberately never copied into QSettings.
    saved.endGroup();
    saved.sync();
    if (saved.status() != QSettings::NoError) {
        if (error) *error = text(u"\u5bc6\u94a5\u5df2\u5b89\u5168\u66f4\u65b0\uff0c\u4f46\u5730\u5740\u548c\u6a21\u578b\u672a\u80fd\u4fdd\u5b58\uff1b\u8bf7\u68c0\u67e5\u7528\u6237\u914d\u7f6e\u6743\u9650\u3002");
        return false;
    }
    return true;
}

bool hasParameterAiSettings(const ParameterAiSettings &settings)
{
    return validEndpoint(settings.endpoint) && validModel(settings.model) && validApiKey(settings.apiKey)
        && !settings.endpoint.contains(settings.apiKey)
        && !QUrl::fromPercentEncoding(settings.endpoint.toUtf8()).contains(settings.apiKey);
}

bool hasParameterAiSettings()
{
    return hasParameterAiSettings(loadParameterAiSettings());
}

} // namespace aerosim
