#include "aerosim/ui/ParameterAiSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <cstddef>

namespace aerosim {
namespace {
template <std::size_t N>
QString text(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}
} // namespace

ParameterAiSettingsDialog::ParameterAiSettingsDialog(QWidget *parent)
    : ParameterAiSettingsDialog(loadParameterAiSettings(), parent)
{
}

ParameterAiSettingsDialog::ParameterAiSettingsDialog(const ParameterAiSettings &settings, QWidget *parent)
    : QDialog(parent), hadKey_(!settings.apiKey.isEmpty())
{
    setObjectName(QStringLiteral("parameterAiSettingsDialog"));
    setWindowTitle(text(u"AI \u8bbe\u7f6e"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(560, 340);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);
    auto *description = new QLabel(text(u"\u914d\u7f6e\u53c2\u6570\u52a9\u624b\u4f7f\u7528\u7684 API \u5730\u5740\u548c\u6a21\u578b\u3002\u5bc6\u94a5\u4ec5\u5728\u672c\u673a\u586b\u5199\u3002"), this);
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setSpacing(10);
    endpoint_ = new QLineEdit(settings.endpoint, this);
    endpoint_->setObjectName(QStringLiteral("parameterAiEndpoint"));
    endpoint_->setAccessibleName(text(u"\u63a5\u53e3\u5730\u5740\uff08Base URL\uff09"));
    endpoint_->setPlaceholderText(QStringLiteral("https://api.site/v1"));
    endpoint_->setMaxLength(2048);
    form->addRow(text(u"\u63a5\u53e3\u5730\u5740\uff08Base URL\uff09"), endpoint_);
    model_ = new QLineEdit(settings.model, this);
    model_->setObjectName(QStringLiteral("parameterAiModel"));
    model_->setAccessibleName(text(u"\u6a21\u578b"));
    model_->setPlaceholderText(text(u"\u586b\u5199\u670d\u52a1\u63d0\u4f9b\u7684\u6a21\u578b\u540d\u79f0"));
    model_->setMaxLength(128);
    model_->setToolTip(text(u"\u6a21\u578b\u987b\u4e3a\u5f53\u524d Key \u6e20\u9053\u652f\u6301\u7684\u540d\u79f0\u3002"));
    form->addRow(text(u"\u6a21\u578b"), model_);
    apiKey_ = new QLineEdit(settings.apiKey, this);
    apiKey_->setObjectName(QStringLiteral("parameterAiApiKey"));
    apiKey_->setAccessibleName(text(u"API \u5bc6\u94a5"));
    apiKey_->setEchoMode(QLineEdit::Password);
    apiKey_->setMaxLength(2560);
    apiKey_->setPlaceholderText(text(u"\u5728\u6b64\u8f93\u5165 API Key"));
    form->addRow(text(u"API \u5bc6\u94a5"), apiKey_);
    layout->addLayout(form);
    auto *showKey = new QCheckBox(text(u"\u663e\u793a\u5bc6\u94a5"), this);
    showKey->setObjectName(QStringLiteral("parameterAiShowKey"));
    connect(showKey, &QCheckBox::toggled, this, [this](bool visible) {
        apiKey_->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    });
    layout->addWidget(showKey);
    clearKey_ = new QCheckBox(text(u"\u79fb\u9664\u5df2\u4fdd\u5b58\u5bc6\u94a5"), this);
    clearKey_->setObjectName(QStringLiteral("parameterAiClearKey"));
    clearKey_->setToolTip(text(u"\u4fdd\u5b58\u65f6\u79fb\u9664\u672c\u673a\u51ed\u636e\uff1b\u73af\u5883\u53d8\u91cf\u4ecd\u53ef\u63d0\u4f9b\u5bc6\u94a5\u3002"));
    clearKey_->setVisible(hadKey_);
    connect(clearKey_, &QCheckBox::toggled, this, [this, showKey](bool clear) {
        if (clear) showKey->setChecked(false);
        apiKey_->setEnabled(!clear);
        showKey->setEnabled(!clear);
    });
    layout->addWidget(clearKey_);
#ifdef Q_OS_WIN
    const QString storage = text(u"\u5bc6\u94a5\u4fdd\u5b58\u5728 Windows \u51ed\u636e\u7ba1\u7406\u5668\u3002");
#else
    const QString storage = text(u"\u5f53\u524d\u7cfb\u7edf\u4ec5\u5728\u672c\u6b21\u5e94\u7528\u4f1a\u8bdd\u4e2d\u4fdd\u7559\u5bc6\u94a5\uff0c\u9000\u51fa\u540e\u9700\u91cd\u65b0\u586b\u5199\u3002");
#endif
    auto *storageNote = new QLabel(storage, this);
    storageNote->setTextFormat(Qt::PlainText);
    storageNote->setWordWrap(true);
    layout->addWidget(storageNote);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("parameterAiStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    status_->setStyleSheet(QStringLiteral("color: #b34436;"));
    status_->hide();
    layout->addWidget(status_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("parameterAiButtons"));
    buttons->button(QDialogButtonBox::Save)->setText(text(u"\u4fdd\u5b58"));
    buttons->button(QDialogButtonBox::Cancel)->setText(text(u"\u53d6\u6d88"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ParameterAiSettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

ParameterAiSettings ParameterAiSettingsDialog::settings() const
{
    return {endpoint_->text().trimmed(), model_->text().trimmed(),
            clearKey_->isChecked() ? QString() : apiKey_->text().trimmed()};
}

void ParameterAiSettingsDialog::save()
{
    if (hadKey_ && apiKey_->text().trimmed().isEmpty() && !clearKey_->isChecked()) {
        status_->setText(text(u"\u8981\u79fb\u9664\u5bc6\u94a5\uff0c\u8bf7\u52fe\u9009\u201c\u79fb\u9664\u5df2\u4fdd\u5b58\u5bc6\u94a5\u201d\u3002"));
        status_->show();
        return;
    }
    QString error;
    if (!saveParameterAiSettings(settings(), &error)) {
        status_->setText(error);
        status_->show();
        return;
    }
    accept();
}

} // namespace aerosim
