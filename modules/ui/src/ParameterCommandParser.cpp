#include "aerosim/ui/ParameterCommandParser.h"

#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <cmath>

namespace aerosim {
namespace {
template <size_t N> QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

struct NumericField {
    QString page;
    QString name;
    QStringList aliases;
    double minimum;
    double maximum;
    bool integer;
    QStringList units;
};

QVector<NumericField> numericFields()
{
    const QString calculation = text(u"\u8ba1\u7b97\u63a7\u5236");
    const QString reference = text(u"\u53c2\u8003\u91cf");
    return {
        {calculation, text(u"\u4e00\u9636\u8fed\u4ee3\u6b65\u6570"), {text(u"\u4e00\u9636\u8fed\u4ee3\u6b65\u6570"), text(u"\u4e00\u9636\u8fed\u4ee3\u6b21\u6570")}, 0, 1e9, true, {text(u"\u6b65"), text(u"\u6b21")}},
        {calculation, text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u6570"), {text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u6570"), text(u"\u6700\u5927\u8fed\u4ee3\u6b21\u6570"), text(u"\u6700\u5927\u8fed\u4ee3\u6570")}, 1, 1e9, true, {text(u"\u6b65"), text(u"\u6b21")}},
        {calculation, text(u"\u6536\u655b\u6b8b\u5dee"), {text(u"\u6536\u655b\u6b8b\u5dee")}, -1e9, 1e9, false, {}},
        {calculation, text(u"\u6b8b\u5dee\u76d1\u63a7\u9608\u503c"), {text(u"\u6b8b\u5dee\u76d1\u63a7\u9608\u503c")}, 0, 1e9, false, {}},
        {calculation, text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570"), {text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570"), text(u"\u8fdb\u7a0b\u6570"), text(u"\u8fdb\u7a0b\u6570\u91cf"), text(u"MPI \u8fdb\u7a0b\u6570")}, 1, 65536, true, {text(u"\u4e2a"), text(u"\u4e2a\u8fdb\u7a0b")}},
        {reference, text(u"\u529b\u77e9\u53c2\u8003\u70b9 X"), {text(u"\u529b\u77e9\u53c2\u8003\u70b9 X"), text(u"\u529b\u77e9\u53c2\u8003\u70b9 X\u5750\u6807")}, -1e9, 1e9, false, {"m", text(u"\u7c73")}},
        {reference, text(u"\u529b\u77e9\u53c2\u8003\u70b9 Y"), {text(u"\u529b\u77e9\u53c2\u8003\u70b9 Y"), text(u"\u529b\u77e9\u53c2\u8003\u70b9 Y\u5750\u6807")}, -1e9, 1e9, false, {"m", text(u"\u7c73")}},
        {reference, text(u"\u529b\u77e9\u53c2\u8003\u70b9 Z"), {text(u"\u529b\u77e9\u53c2\u8003\u70b9 Z"), text(u"\u529b\u77e9\u53c2\u8003\u70b9 Z\u5750\u6807")}, -1e9, 1e9, false, {"m", text(u"\u7c73")}},
        {reference, text(u"\u6c14\u52a8\u5f26\u957f (m)"), {text(u"\u6c14\u52a8\u5f26\u957f"), text(u"\u53c2\u8003\u5f26\u957f"), text(u"\u5f26\u957f")}, 0.000001, 1e9, false, {"m", text(u"\u7c73")}},
        {reference, text(u"\u6c14\u52a8\u5c55\u957f (m)"), {text(u"\u6c14\u52a8\u5c55\u957f"), text(u"\u53c2\u8003\u5c55\u957f"), text(u"\u5c55\u957f")}, 0.000001, 1e9, false, {"m", text(u"\u7c73")}},
        {reference, text(u"\u53c2\u8003\u9762\u79ef (m\u00b2)"), {text(u"\u53c2\u8003\u9762\u79ef")}, 0.000001, 1e12, false, {text(u"m\u00b2"), "m2", text(u"\u5e73\u65b9\u7c73")}},
        {reference, text(u"\u9a6c\u8d6b\u6570"), {text(u"\u9a6c\u8d6b\u6570"), text(u"\u9a6c\u8d6b"), "Mach"}, 0, 1e4, false, {}},
        {reference, text(u"\u653b\u89d2 (\u00b0)"), {text(u"\u653b\u89d2"), text(u"\u8fce\u89d2")}, -180, 180, false, {text(u"\u5ea6"), text(u"\u00b0"), "deg"}},
        {reference, text(u"\u4fa7\u6ed1\u89d2 (\u00b0)"), {text(u"\u4fa7\u6ed1\u89d2")}, -180, 180, false, {text(u"\u5ea6"), text(u"\u00b0"), "deg"}}
    };
}

QString alternatives(const QStringList &values)
{
    QStringList patterns;
    for (const auto &value : values) {
        QStringList words;
        for (const auto &word : value.split(QLatin1Char(' '), QString::SkipEmptyParts))
            words.append(QRegularExpression::escape(word));
        patterns.append(words.join(QStringLiteral("\\s*")));
    }
    return QStringLiteral("(?:") + patterns.join(QLatin1Char('|')) + QLatin1Char(')');
}

QString assignment()
{
    return text(u"(?:\u8bbe\u7f6e\u4e3a|\u8bbe\u7f6e\u6210|\u8bbe\u4e3a|\u8bbe\u6210|\u6539\u4e3a|\u6539\u6210|\u8c03\u6574\u4e3a|\u8c03\u6574\u5230|\u8bbe\u7f6e|\u4e3a|\u662f|=|:|\uff1a)?");
}

ParameterCommandParseResult error(const QString &message)
{
    ParameterCommandParseResult result;
    result.error = message;
    return result;
}

bool parseGpu(const QString &clause, QString *value)
{
    QString compact = clause;
    compact.remove(QRegularExpression(QStringLiteral("\\s+")));
    const QString enabled = text(u"(?:\u542f\u7528|\u5f00\u542f|\u6253\u5f00|\u4f7f\u7528|\u662f|\u5f00|true|1|\u4f7f\u7528GPU\u8ba1\u7b97)");
    const QString disabled = text(u"(?:\u5173\u95ed|\u7981\u7528|\u505c\u7528|\u4e0d\u4f7f\u7528|\u5426|\u5173|false|0|\u4e0d\u4f7f\u7528GPU\u8ba1\u7b97)");
    const auto matches = [&](const QString &pattern) {
        return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption).match(compact).hasMatch();
    };
    if (matches(text(u"^(?:GPU\u8ba1\u7b97|GPU)") + assignment() + enabled + QLatin1Char('$'))
        || matches(text(u"^(?:\u542f\u7528|\u5f00\u542f|\u6253\u5f00|\u4f7f\u7528)GPU(?:\u8ba1\u7b97)?$"))) {
        *value = text(u"\u4f7f\u7528GPU\u8ba1\u7b97");
        return true;
    }
    if (matches(text(u"^(?:GPU\u8ba1\u7b97|GPU)") + assignment() + disabled + QLatin1Char('$'))
        || matches(text(u"^(?:\u5173\u95ed|\u7981\u7528|\u505c\u7528|\u4e0d\u4f7f\u7528)GPU(?:\u8ba1\u7b97)?$"))) {
        *value = text(u"\u4e0d\u4f7f\u7528GPU\u8ba1\u7b97");
        return true;
    }
    return false;
}

// Count effective decimal places before converting to double. This also rejects
// precision that toDouble would silently discard, e.g. 1.00000000000000001.
bool hasSupportedPrecision(QString literal, bool integer)
{
    if (literal.startsWith(QLatin1Char('+')) || literal.startsWith(QLatin1Char('-'))) literal.remove(0, 1);
    const int e = literal.indexOf(QLatin1Char('e'), 0, Qt::CaseInsensitive);
    int exponent = 0;
    if (e >= 0) {
        bool ok = false;
        exponent = literal.mid(e + 1).toInt(&ok);
        if (!ok || exponent < -1000 || exponent > 1000) return false;
        literal = literal.left(e);
    }
    const int point = literal.indexOf(QLatin1Char('.'));
    int decimals = point < 0 ? 0 : literal.size() - point - 1;
    literal.remove(QLatin1Char('.'));
    while (literal.endsWith(QLatin1Char('0'))) {
        literal.chop(1);
        --decimals;
    }
    if (literal.isEmpty()) return true;
    return decimals - exponent <= (integer ? 0 : 6);
}
}

ParameterCommandParseResult parseParameterCommand(const QString &instruction)
{
    if (instruction.trimmed().isEmpty()) return error(text(u"\u8bf7\u8f93\u5165\u8981\u914d\u7f6e\u7684\u53c2\u6570\uff0c\u4f8b\u5982\uff1a\u6700\u5927\u8fed\u4ee3\u6b65\u6570\u8bbe\u4e3a12000\uff0c\u8fdb\u7a0b\u65706\u3002"));
    if (instruction.size() > 2048) return error(text(u"\u6307\u4ee4\u8fc7\u957f\uff0c\u8bf7\u9650\u5236\u57282048\u5b57\u4ee5\u5185\u3002"));
    QString source = instruction.trimmed();
    if (source.endsWith(text(u"\u3002"))) source.chop(1);
    source.replace(QChar(0x2212), QLatin1Char('-'));
    const auto clauses = source.split(QRegularExpression(text(u"[\uff0c,\uff1b;\\r\\n]+")), QString::SkipEmptyParts);
    if (clauses.size() > 24) return error(text(u"\u4e00\u6b21\u6700\u591a\u914d\u7f6e24\u6761\u53c2\u6570\u3002"));
    QJsonObject values;
    QString page;
    QStringList summary;
    const auto fields = numericFields();
    const QString numeric = QStringLiteral("([+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?)");
    const QRegularExpression prefix(text(u"^(?:(?:\u8bf7(?:\u5e2e\u6211)?|\u5e2e\u6211)?(?:\u5c06|\u628a)?(?:\u8bbe\u7f6e|\u914d\u7f6e)?)\\s*"));
    const QRegularExpression pagePrefix(text(u"^(\u8ba1\u7b97\u63a7\u5236|\u53c2\u8003\u91cf)(?:\u53c2\u6570)?(?:\u9875\u9762)?(?:\u91cc\u7684|\u4e2d\u7684|\u91cc|\u4e2d|\u7684)?\\s*[:\uff1a]?\\s*"));
    for (auto clause : clauses) {
        clause = clause.trimmed();
        clause.remove(prefix);
        const auto header = pagePrefix.match(clause);
        if (header.hasMatch()) {
            const QString declaredPage = header.captured(1);
            if (!page.isEmpty() && page != declaredPage) return error(text(u"\u4e00\u6b21\u6307\u4ee4\u53ea\u80fd\u914d\u7f6e\u4e00\u4e2a\u53c2\u6570\u9875\u9762\uff0c\u8bf7\u5c06\u8ba1\u7b97\u63a7\u5236\u4e0e\u53c2\u8003\u91cf\u5206\u5f00\u3002"));
            page = declaredPage;
            clause.remove(0, header.capturedLength());
            clause.remove(prefix);
            if (clause.isEmpty()) continue;
        }
        QString gpu;
        QString fieldPage;
        QString fieldName;
        QJsonValue fieldValue;
        if (parseGpu(clause, &gpu)) {
            fieldPage = text(u"\u8ba1\u7b97\u63a7\u5236");
            fieldName = text(u"GPU \u8ba1\u7b97");
            fieldValue = gpu;
        } else {
            for (const auto &field : fields) {
                const QString units = field.units.isEmpty() ? QString() : alternatives(field.units) + QLatin1Char('?');
                const QString pattern = QLatin1Char('^') + alternatives(field.aliases) + QStringLiteral("\\s*")
                    + assignment() + QStringLiteral("\\s*") + numeric + QStringLiteral("\\s*") + units + QLatin1Char('$');
                const auto match = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption).match(clause);
                if (!match.hasMatch()) continue;
                bool valid = false;
                const QString literal = match.captured(1);
                const double value = literal.toDouble(&valid);
                if (!valid || !qIsFinite(value) || value < field.minimum || value > field.maximum) {
                    return error(text(u"%1\u8d85\u51fa\u6709\u6548\u8303\u56f4 [%2, %3]\u3002")
                        .arg(field.name).arg(field.minimum, 0, 'g', 15).arg(field.maximum, 0, 'g', 15));
                }
                if (!hasSupportedPrecision(literal, field.integer)) {
                    return error(field.integer ? text(u"%1\u5fc5\u987b\u4e3a\u6574\u6570\uff0c\u4e0d\u4f1a\u81ea\u52a8\u53d6\u6574\u3002").arg(field.name)
                        : text(u"%1\u6700\u591a\u652f\u63016\u4f4d\u5c0f\u6570\uff0c\u4e0d\u4f1a\u81ea\u52a8\u820d\u5165\u3002").arg(field.name));
                }
                fieldPage = field.page;
                fieldName = field.name;
                fieldValue = value;
                break;
            }
        }
        if (fieldName.isEmpty()) return error(text(u"\u65e0\u6cd5\u5b8c\u6574\u8bc6\u522b\u201c%1\u201d\u3002\u8bf7\u4f7f\u7528\u53c2\u6570\u540d\u548c\u6570\u503c\uff0c\u5e76\u7528\u9017\u53f7\u5206\u9694\uff1b\u4e0d\u652f\u6301\u8ba1\u7b97\u3001\u4fdd\u5b58\u7b49\u64cd\u4f5c\u3002").arg(clause));
        if (!page.isEmpty() && page != fieldPage) return error(text(u"\u4e00\u6b21\u6307\u4ee4\u53ea\u80fd\u914d\u7f6e\u4e00\u4e2a\u53c2\u6570\u9875\u9762\uff0c\u8bf7\u5c06\u8ba1\u7b97\u63a7\u5236\u4e0e\u53c2\u8003\u91cf\u5206\u5f00\u3002"));
        page = fieldPage;
        if (values.contains(fieldName)) return error(text(u"\u53c2\u6570\u201c%1\u201d\u91cd\u590d\u51fa\u73b0\uff0c\u8bf7\u6bcf\u4e2a\u53c2\u6570\u53ea\u8bbe\u7f6e\u4e00\u6b21\u3002").arg(fieldName));
        values.insert(fieldName, fieldValue);
        summary.append(fieldName + QStringLiteral(" = ") + (fieldValue.isString() ? fieldValue.toString()
            : QString::number(fieldValue.toDouble(), 'g', 15)));
    }
    if (values.isEmpty()) return error(text(u"\u6ca1\u6709\u8bc6\u522b\u5230\u53c2\u6570\u503c\uff0c\u8bf7\u5728\u9875\u9762\u540d\u79f0\u540e\u586b\u5199\u53c2\u6570\u540d\u548c\u6570\u503c\u3002"));
    ParameterCommandParseResult result;
    result.ok = true;
    result.preset = {{QStringLiteral("page"), page}, {QStringLiteral("values"), values}};
    result.summary = page + text(u"\uff1a") + summary.join(text(u"\uff1b"));
    return result;
}

bool validateParameterPreset(const QJsonObject &preset, QString *message)
{
    const auto reject = [message](const QString &reason) {
        if (message) *message = reason;
        return false;
    };
    if (message) message->clear();
    if (preset.size() != 2 || !preset.value("page").isString() || !preset.value("values").isObject())
        return reject(text(u"\u53c2\u6570\u65b9\u6848\u683c\u5f0f\u65e0\u6548\u3002"));
    const QString page = preset.value("page").toString();
    if (page != text(u"\u8ba1\u7b97\u63a7\u5236") && page != text(u"\u53c2\u8003\u91cf"))
        return reject(text(u"\u5f53\u524d\u4ec5\u652f\u6301\u8ba1\u7b97\u63a7\u5236\u548c\u53c2\u8003\u91cf\u3002"));
    const QJsonObject values = preset.value("values").toObject();
    if (values.isEmpty()) return reject(text(u"\u53c2\u6570\u65b9\u6848\u4e2d\u6ca1\u6709\u6570\u503c\u3002"));
    const auto fields = numericFields();
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (page == text(u"\u8ba1\u7b97\u63a7\u5236") && it.key() == text(u"GPU \u8ba1\u7b97")) {
            if (!it.value().isString() || (it.value().toString() != text(u"\u4f7f\u7528GPU\u8ba1\u7b97")
                && it.value().toString() != text(u"\u4e0d\u4f7f\u7528GPU\u8ba1\u7b97"))) return reject(text(u"GPU\u9009\u9879\u65e0\u6548\u3002"));
            continue;
        }
        bool matched = false;
        for (const auto &field : fields) {
            if (field.page != page || field.name != it.key()) continue;
            matched = true;
            if (!it.value().isDouble()) return reject(text(u"\u53c2\u6570\u5fc5\u987b\u4f7f\u7528\u6570\u5b57\uff1a%1").arg(it.key()));
            const double number = it.value().toDouble();
            if (!qIsFinite(number) || number < field.minimum || number > field.maximum
                || (field.integer && std::floor(number) != number)
                || !hasSupportedPrecision(QString::number(number, 'g', 15), field.integer)
                || (!field.integer && QString::number(number, 'f', 6).toDouble() != number))
                return reject(text(u"\u53c2\u6570\u6570\u503c\u3001\u8303\u56f4\u6216\u7cbe\u5ea6\u65e0\u6548\uff1a%1").arg(it.key()));
            break;
        }
        if (!matched) return reject(text(u"\u6682\u4e0d\u652f\u6301\u53c2\u6570\uff1a%1").arg(it.key()));
    }
    return true;
}

QString parameterNumberText(double value)
{
    QString result = QString::number(value, 'f', 6);
    while (result.endsWith(QLatin1Char('0'))) result.chop(1);
    if (result.endsWith(QLatin1Char('.'))) result.chop(1);
    return result;
}

} // namespace aerosim
