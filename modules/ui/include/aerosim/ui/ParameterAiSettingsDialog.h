#pragma once

#include "aerosim/ui/ParameterAiSettings.h"
#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace aerosim {

class ParameterAiSettingsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ParameterAiSettingsDialog(QWidget *parent = nullptr);
    explicit ParameterAiSettingsDialog(const ParameterAiSettings &settings, QWidget *parent = nullptr);
    ParameterAiSettings settings() const;

private:
    void save();
    QLineEdit *endpoint_ = nullptr;
    QLineEdit *model_ = nullptr;
    QLineEdit *apiKey_ = nullptr;
    QCheckBox *clearKey_ = nullptr;
    QLabel *status_ = nullptr;
    bool hadKey_ = false;
};

} // namespace aerosim
