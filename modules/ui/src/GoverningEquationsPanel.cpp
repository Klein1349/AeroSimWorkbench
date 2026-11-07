#include "aerosim/ui/GoverningEquationsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N>
QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}
}


GoverningEquationsPanel::GoverningEquationsPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft)
{
    setObjectName(QStringLiteral("governingEquationsPanel"));
    draft_.insert("calculationType", draft.value("calculationType", "unsteady"));
    draft_.insert("equation", draft.value("equation", "euler"));
    draft_.insert("flowRegime", draft.value("flowRegime", "inviscid"));
    draft_.insert("rarefiedGas", draft.value("rarefiedGas", false));
    setStyleSheet(QStringLiteral(
        "QWidget#governingEquationsPanel QGroupBox {"
        " background: white; border: 1px solid #d7e1ed; border-radius: 6px;"
        " margin-top: 12px; padding: 18px 14px 12px; color: #234d7a; font-weight: 600; }"
        "QWidget#governingEquationsPanel QGroupBox::title {"
        " subcontrol-origin: margin; subcontrol-position: top left; left: 14px; padding: 0 5px; }"
        "QWidget#governingEquationsPanel QRadioButton,"
        "QWidget#governingEquationsPanel QCheckBox { color: #25364c; font-weight: normal; spacing: 8px; min-height: 26px; }"
        "QWidget#governingEquationsPanel QComboBox { min-height: 30px; padding-left: 8px; }"
        "QWidget#governingEquationsPanel QLabel { color: #52677f; font-weight: normal; }"));
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    auto *heading = new QLabel(uiText(u"基础参数"), this);
    heading->setStyleSheet(QStringLiteral("color: #52677f; font-size: 12px;"));
    layout->addWidget(heading, 0, 0, 1, 2);

    auto *time = new QGroupBox(uiText(u"时间项"), this);
    auto *timeLayout = new QFormLayout(time);
    timeLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    timeLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *calculationType = new QComboBox(time);
    calculationType->setObjectName(QStringLiteral("calculationType"));
    calculationType->addItem(uiText(u"定常计算"), QStringLiteral("steady"));
    calculationType->addItem(uiText(u"非定常计算"), QStringLiteral("unsteady"));
    calculationType->setCurrentIndex(calculationType->findData(draft_.value("calculationType")));
    timeLayout->addRow(uiText(u"计算类型"), calculationType);
    connect(calculationType, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, calculationType] { updateDraft("calculationType", calculationType->currentData()); });
    layout->addWidget(time, 1, 0);

    auto *equations = new QGroupBox(uiText(u"方程设置"), this);
    auto *equationsLayout = new QVBoxLayout(equations);
    equationsLayout->setSpacing(6);
    equationsLayout->addWidget(new QLabel(uiText(u"求解方程"), equations));
    auto *euler = new QRadioButton(uiText(u"欧拉方程"), equations);
    euler->setObjectName(QStringLiteral("eulerEquation"));
    auto *navierStokes = new QRadioButton(uiText(u"N-S 方程"), equations);
    navierStokes->setObjectName(QStringLiteral("navierStokesEquation"));
    euler->setChecked(draft_.value("equation") == QStringLiteral("euler"));
    navierStokes->setChecked(draft_.value("equation") == QStringLiteral("navier-stokes"));
    equationsLayout->addWidget(euler);
    equationsLayout->addWidget(navierStokes);
    connect(euler, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("equation", QStringLiteral("euler"));
    });
    connect(navierStokes, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("equation", QStringLiteral("navier-stokes"));
    });
    layout->addWidget(equations, 1, 1);

    auto *flow = new QGroupBox(uiText(u"流态设置"), this);
    auto *flowLayout = new QVBoxLayout(flow);
    auto *inviscid = new QRadioButton(uiText(u"无粘"), flow);
    inviscid->setObjectName(QStringLiteral("inviscidFlow"));
    inviscid->setChecked(true);
    flowLayout->addWidget(inviscid);
    layout->addWidget(flow, 2, 0);

    auto *rarefied = new QGroupBox(uiText(u"稀薄气体效应"), this);
    auto *rarefiedLayout = new QVBoxLayout(rarefied);
    auto *enableRarefied = new QCheckBox(uiText(u"考虑稀薄气体效应"), rarefied);
    enableRarefied->setObjectName(QStringLiteral("rarefiedGasEffect"));
    enableRarefied->setChecked(draft_.value("rarefiedGas").toBool());
    rarefiedLayout->addWidget(enableRarefied);
    connect(enableRarefied, &QCheckBox::toggled, this, [this](bool checked) {
        updateDraft("rarefiedGas", checked);
    });
    layout->addWidget(rarefied, 2, 1);
}

void GoverningEquationsPanel::updateDraft(const QString &key, const QVariant &value)
{
    draft_.insert(key, value);
    emit draftChanged(draft_);
}

} // namespace aerosim
