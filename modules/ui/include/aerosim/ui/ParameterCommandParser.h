#pragma once

#include <QJsonObject>
#include <QString>

namespace aerosim {

struct ParameterCommandParseResult {
    bool ok = false;
    QJsonObject preset;
    QString summary;
    QString error;
};

// Parses the supported parameter clauses only; never performs UI or file operations.
// On any invalid clause the result has no preset, including previously parsed fields.
ParameterCommandParseResult parseParameterCommand(const QString &instruction);
// Validates a structured model result against the same fields and widget ranges.
bool validateParameterPreset(const QJsonObject &preset, QString *error = nullptr);
QString parameterNumberText(double value);

} // namespace aerosim
