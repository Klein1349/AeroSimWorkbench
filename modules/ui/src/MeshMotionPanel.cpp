#include "aerosim/ui/MeshMotionPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}
}

MeshMotionPanel::MeshMotionPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft), bodies_(draft.value("bodies").toList())
{
    setObjectName(QStringLiteral("meshMotionPanel"));
    setStyleSheet(QStringLiteral(
        "QWidget#meshMotionPanel QTableWidget { background: white; border: 1px solid #d7e1ed; border-radius: 5px; }"
        "QWidget#meshMotionPanel QHeaderView::section { background: #eaf0f7; color: #234d7a; padding: 8px; border: 0; }"
        "QWidget#meshMotionPanel QGroupBox { background: white; border: 1px solid #d7e1ed; border-radius: 6px;"
        " margin-top: 12px; padding: 18px 14px 12px; color: #234d7a; }"
        "QWidget#meshMotionPanel QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; }"
        "QWidget#meshMotionPanel QLineEdit, QWidget#meshMotionPanel QComboBox,"
        "QWidget#meshMotionPanel QAbstractSpinBox { min-height: 28px; }"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    enabled_ = new QCheckBox(uiText(u"是否启动动网格"), this);
    enabled_->setObjectName(QStringLiteral("meshMotionEnabled"));
    enabled_->setChecked(draft.value("enabled", false).toBool());
    layout->addWidget(enabled_);
    auto *countForm = new QFormLayout;
    count_ = new QSpinBox(this);
    count_->setObjectName(QStringLiteral("motionBodyCount"));
    count_->setRange(0, 100);
    count_->setValue(draft.value("bodyCount", 0).toInt());
    count_->setMaximumWidth(220);
    count_->setEnabled(enabled_->isChecked());
    countForm->addRow(uiText(u"运动体数目"), count_);
    layout->addLayout(countForm);
    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("motionBodyTable"));
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({uiText(u"运动体名称"), uiText(u"指定运动体"), uiText(u"运动类型"), uiText(u"参数设置")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setMinimumSectionSize(120);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(42);
    table_->setMinimumHeight(200);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setEnabled(enabled_->isChecked());
    layout->addWidget(table_);
    parameters_ = new QGroupBox(uiText(u"运动参数"), this);
    parameters_->setObjectName(QStringLiteral("motionParameters"));
    auto *form = new QFormLayout(parameters_);
    parameterHint_ = new QLabel(parameters_);
    parameterHint_->setWordWrap(true);
    form->addRow(parameterHint_);
    for (int axis = 0; axis < 3; ++axis) {
        auto *component = new QDoubleSpinBox(parameters_);
        components_[axis] = component;
        component->setObjectName(QStringLiteral("motionComponent%1").arg(axis));
        component->setDecimals(6);
        component->setRange(-1e9, 1e9);
        const QString axisName = QString(QChar('X' + axis));
        form->addRow(axisName, component);
        connect(component, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, axis](double value) {
            if (selectedRow_ < 0) return;
            const QString key = bodies_.at(selectedRow_).toMap().value("type").toString() == "rotation" ? "angularVelocity" : "velocity";
            auto values = bodies_.at(selectedRow_).toMap().value(key).toList();
            values[axis] = value;
            updateBody(selectedRow_, key, values);
        });
    }
    layout->addWidget(parameters_);
    auto *hint = new QLabel(uiText(u"运动体和参数为本地配置草稿；指定运动体填写网格或对象名称，真实对象绑定与动网格求解尚未接入。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #6b7c90;"));
    layout->addWidget(hint);
    // Populate without emitting a user edit during page construction.
    while (bodies_.size() < count_->value()) {
        const int index = bodies_.size() + 1;
        bodies_.append(QVariantMap{{"name", QStringLiteral("motion_body_%1").arg(index)}, {"target", ""},
            {"type", "translation"}, {"velocity", QVariantList{0., 0., 0.}}, {"angularVelocity", QVariantList{0., 0., 0.}}});
    }
    rebuildTable();
    connect(enabled_, &QCheckBox::toggled, this, [this](bool enabled) {
        count_->setEnabled(enabled);
        table_->setEnabled(enabled);
        parameters_->setEnabled(enabled);
        publish();
    });
    connect(count_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MeshMotionPanel::resizeBodies);
    connect(table_, &QTableWidget::currentCellChanged, this, [this](int row) { selectBody(row); });
}

void MeshMotionPanel::resizeBodies(int count)
{
    // Keep inactive bodies so reducing and restoring the count does not lose edits.
    while (bodies_.size() < count) {
        bodies_.append(QVariantMap{{"name", QStringLiteral("motion_body_%1").arg(bodies_.size() + 1)}, {"target", ""},
            {"type", "translation"}, {"velocity", QVariantList{0., 0., 0.}}, {"angularVelocity", QVariantList{0., 0., 0.}}});
    }
    rebuildTable();
    publish();
}

void MeshMotionPanel::rebuildTable()
{
    const QSignalBlocker blocker(table_);
    table_->setRowCount(0);
    table_->setRowCount(count_->value());
    for (int row = 0; row < count_->value(); ++row) {
        const auto body = bodies_.at(row).toMap();
        for (int column = 0; column < 2; ++column) {
            const QString key = column == 0 ? "name" : "target";
            auto *edit = new QLineEdit(body.value(key).toString(), table_);
            edit->setObjectName(QStringLiteral("motionBody%1_%2").arg(row).arg(key));
            edit->setMaxLength(128);
            if (column == 1) edit->setPlaceholderText(uiText(u"填写网格 / 对象名称"));
            table_->setCellWidget(row, column, edit);
            connect(edit, &QLineEdit::textChanged, this, [this, row, key](const QString &value) { updateBody(row, key, value); });
        }
        auto *type = new QComboBox(table_);
        type->setObjectName(QStringLiteral("motionBody%1_type").arg(row));
        type->addItem(uiText(u"平移"), QStringLiteral("translation"));
        type->addItem(uiText(u"旋转"), QStringLiteral("rotation"));
        type->setCurrentIndex(type->findData(body.value("type")));
        table_->setCellWidget(row, 2, type);
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, row, type] {
            updateBody(row, "type", type->currentData());
            selectBody(row);
        });
        auto *settings = new QPushButton(uiText(u"设置参数"), table_);
        settings->setObjectName(QStringLiteral("motionBody%1_settings").arg(row));
        table_->setCellWidget(row, 3, settings);
        connect(settings, &QPushButton::clicked, this, [this, row] {
            table_->setCurrentCell(row, 3);
            selectBody(row);
        });
    }
    selectBody(-1);
}

void MeshMotionPanel::selectBody(int row)
{
    selectedRow_ = row >= 0 && row < count_->value() ? row : -1;
    parameters_->setVisible(selectedRow_ >= 0);
    parameters_->setEnabled(enabled_->isChecked());
    if (selectedRow_ < 0) return;
    const auto body = bodies_.at(row).toMap();
    const bool rotation = body.value("type").toString() == "rotation";
    parameters_->setTitle(uiText(u"运动参数 · ") + body.value("name").toString());
    parameterHint_->setText(rotation ? uiText(u"绕全局原点的角速度分量（右手定则）") : uiText(u"全局坐标系中的平移速度分量"));
    const auto values = body.value(rotation ? "angularVelocity" : "velocity").toList();
    for (int axis = 0; axis < 3; ++axis) {
        const QSignalBlocker blocker(components_[axis]);
        components_[axis]->setSuffix(rotation ? QStringLiteral(" rad/s") : QStringLiteral(" m/s"));
        components_[axis]->setValue(values.value(axis, 0.).toDouble());
    }
}

void MeshMotionPanel::updateBody(int row, const QString &key, const QVariant &value)
{
    auto body = bodies_.at(row).toMap();
    body.insert(key, value);
    bodies_[row] = body;
    publish();
}

void MeshMotionPanel::publish()
{
    draft_.insert("enabled", enabled_->isChecked());
    draft_.insert("bodyCount", count_->value());
    draft_.insert("bodies", bodies_);
    emit draftChanged(draft_);
}

} // namespace aerosim
