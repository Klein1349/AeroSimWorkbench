#pragma once

#include <QString>

namespace aerosim {

struct ParameterAiSettings {
    QString endpoint;
    QString model;
    QString apiKey;
};

ParameterAiSettings loadParameterAiSettings();
// An empty apiKey explicitly removes the saved credential. Callers must make this clear to users.
bool saveParameterAiSettings(const ParameterAiSettings &settings, QString *error = nullptr);
// Pure validation for callers that already hold settings; never reads credentials.
bool hasParameterAiSettings(const ParameterAiSettings &settings);
bool hasParameterAiSettings();

} // namespace aerosim
