#include "aerosim/ui/ParameterCommandParser.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QTextStream>

namespace {
template <size_t N> QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}
bool check(bool condition, const char *message)
{
    if (!condition) QTextStream(stderr) << message << '\n';
    return condition;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto calculation = aerosim::parseParameterCommand(text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u6570\u8bbe\u4e3a12000\uff0c\u8fdb\u7a0b\u65706\uff0cGPU\u542f\u7528"));
    const auto calculationValues = calculation.preset.value("values").toObject();
    if (!check(calculation.ok && calculation.error.isEmpty() && !calculation.summary.isEmpty()
        && calculation.preset.value("page").toString() == text(u"\u8ba1\u7b97\u63a7\u5236")
        && calculationValues.value(text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u6570")).toDouble() == 12000
        && calculationValues.value(text(u"\u8ba1\u7b97\u8fdb\u7a0b\u6570")).toDouble() == 6
        && calculationValues.value(text(u"GPU \u8ba1\u7b97")).toString() == text(u"\u4f7f\u7528GPU\u8ba1\u7b97"), "Calculation command did not produce the expected preset")) return 1;
    const auto reference = aerosim::parseParameterCommand(text(u"\u8bf7\u628a\u53c2\u8003\u91cf\u91cc\u7684\u9a6c\u8d6b\u6570\u8bbe\u4e3a0.8\uff0c\u653b\u89d25\u5ea6\uff0c\u4fa7\u6ed1\u89d2-2.5e1\u5ea6"));
    const auto referenceValues = reference.preset.value("values").toObject();
    if (!check(reference.ok && reference.preset.value("page").toString() == text(u"\u53c2\u8003\u91cf")
        && referenceValues.value(text(u"\u9a6c\u8d6b\u6570")).toDouble() == 0.8
        && referenceValues.value(text(u"\u653b\u89d2 (\u00b0)")).toDouble() == 5
        && referenceValues.value(text(u"\u4fa7\u6ed1\u89d2 (\u00b0)")).toDouble() == -25, "Reference aliases, units or scientific notation failed")) return 2;
    for (const auto &command : {text(u"\u5173\u95edGPU"), text(u"GPU\u8bbe\u4e3afalse"), text(u"\u4e0d\u4f7f\u7528GPU\u8ba1\u7b97")}) {
        const auto parsed = aerosim::parseParameterCommand(command);
        if (!check(parsed.ok && parsed.preset.value("values").toObject().value(text(u"GPU \u8ba1\u7b97")).toString() == text(u"\u4e0d\u4f7f\u7528GPU\u8ba1\u7b97"), "GPU disable alias failed")) return 3;
    }
    const auto precision = aerosim::parseParameterCommand(text(u"\u53c2\u8003\u9762\u79ef1e-6\u5e73\u65b9\u7c73\uff0c\u529b\u77e9\u53c2\u8003\u70b9X -1.23000000m\uff0c\u5f26\u957f1.0e1\u7c73"));
    if (!check(precision.ok && precision.preset.value("values").toObject().value(text(u"\u529b\u77e9\u53c2\u8003\u70b9 X")).toDouble() == -1.23,
               "Valid exact decimal precision was rejected")) return 4;
    const QStringList rejected = {
        QString(), text(u"\u8ba1\u7b97\u63a7\u5236"), text(u"\u542f\u52a8\u8ba1\u7b97"),
        text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u657012000\uff0c\u542f\u52a8\u8ba1\u7b97"), text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u657012000\u7136\u540e\u4fdd\u5b58"),
        text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u657012000\uff0c\u9a6c\u8d6b\u65700.8"), text(u"\u8ba1\u7b97\u63a7\u5236\uff1a\u9a6c\u8d6b\u65700.8"),
        text(u"\u8fdb\u7a0b\u65706\uff0c\u8ba1\u7b97\u8fdb\u7a0b\u65706"), text(u"GPU\u542f\u7528\uff0c\u5173\u95edGPU"),
        text(u"\u8fdb\u7a0b\u65700"), text(u"\u8fdb\u7a0b\u657065537"), text(u"\u8fdb\u7a0b\u65706.1"),
        text(u"\u6700\u5927\u8fed\u4ee3\u6b65\u65701e10000"), text(u"\u653b\u89d2181\u5ea6"), text(u"\u53c2\u8003\u9762\u79ef0"),
        text(u"\u9a6c\u8d6b\u65700.1234567"), text(u"\u9a6c\u8d6b\u65701.00000000000000001"),
        text(u"\u9a6c\u8d6b\u65701e-7"), text(u"\u8fdb\u7a0b\u65706\u4e2a\u7ebf\u7a0b"), text(u"\u9a6c\u8d6b\u6570NaN"),
        text(u"\u9a6c\u8d6b\u65700.8\uff0c\u653b\u89d25\u5ea6\uff0c\u8c22\u8c22"), text(u"\u9a6c\u8d6b\u65700.8m"),
        text(u"\u8fdb\u7a0b\u65701 2"), text(u"GPU"), QString(2049, QLatin1Char('x'))
    };
    for (const auto &command : rejected) {
        const auto parsed = aerosim::parseParameterCommand(command);
        if (!check(!parsed.ok && parsed.preset.isEmpty() && parsed.summary.isEmpty() && !parsed.error.isEmpty(),
                   "Invalid command was accepted or exposed a partial preset")) {
            QTextStream(stderr) << command << '\n';
            return 5;
        }
    }
    return 0;
}
