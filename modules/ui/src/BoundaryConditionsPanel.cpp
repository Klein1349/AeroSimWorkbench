#include "aerosim/ui/BoundaryConditionsPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}

QVariantList sampleBoundaries()
{
    QVariantList rows;
    for (const QString &name : {QStringLiteral("symmetry"), QStringLiteral("wing_up"),
                               QStringLiteral("wing_edge"), QStringLiteral("wing_down"), QStringLiteral("farfield")}) {
        const QString type = name == "symmetry" ? "symmetry" : name == "farfield" ? "farfield" : "inviscid-wall";
        rows.append(QVariantMap{{"id", name}, {"name", name}, {"type", type}, {"mesh", "M6"}});
    }
    return rows;
}
}

BoundaryConditionsPanel::BoundaryConditionsPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft)
{
    setObjectName(QStringLiteral("boundaryConditionsPanel"));
    draft_.insert("boundaries", draft.value("boundaries", sampleBoundaries()));
    draft_.insert("sampleData", true);
    setStyleSheet(QStringLiteral(
        "QWidget#boundaryConditionsPanel QTreeWidget { background: white; border: 1px solid #d7e1ed; border-radius: 5px; }"
        "QWidget#boundaryConditionsPanel QTreeWidget::item { padding: 7px 8px; }"
        "QWidget#boundaryConditionsPanel QHeaderView::section { background: #eaf0f7; color: #234d7a; padding: 8px; border: 0; }"
        "QWidget#boundaryConditionsPanel QGroupBox { background: white; border: 1px solid #d7e1ed; border-radius: 6px;"
        " margin-top: 12px; padding: 18px 14px 12px; color: #234d7a; }"
        "QWidget#boundaryConditionsPanel QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; }"
        "QWidget#boundaryConditionsPanel QLineEdit, QWidget#boundaryConditionsPanel QComboBox { min-height: 28px; }"
        "QPushButton#applyBoundary { background: #2563b6; border: 1px solid #2563b6; color: white; border-radius: 4px; padding: 7px 24px; }"
        "QPushButton#applyBoundary:disabled { background: #e9eef5; color: #8290a2; border-color: #d7e1ed; }"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *sample = new QLabel(uiText(u"M6 示例边界 · 尚未从实际网格读取"), this);
    sample->setStyleSheet(QStringLiteral("color: #6b7c90;"));
    layout->addWidget(sample);
    auto *content = new QHBoxLayout;
    content->setSpacing(16);
    table_ = new QTreeWidget(this);
    table_->setObjectName(QStringLiteral("boundaryTable"));
    table_->setHeaderLabels({uiText(u"边界名称"), uiText(u"边界类型"), uiText(u"所属网格")});
    table_->setRootIsDecorated(false);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setMinimumHeight(180);
    table_->header()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSortingEnabled(false);
    QTreeWidgetItem *initial = nullptr;
    for (const QVariant &value : draft_.value("boundaries").toList()) {
        const auto row = value.toMap();
        auto *item = new QTreeWidgetItem(table_, {row.value("name").toString(), typeLabel(row.value("type").toString()), row.value("mesh").toString()});
        item->setData(0, Qt::UserRole, row.value("id"));
        item->setData(1, Qt::UserRole, row.value("type"));
        item->setIcon(1, QIcon(QStringLiteral(":/aerosim/postprocess/parameters.svg")));
        if (row.value("id").toString() == draft_.value("selectedId", "wing_edge").toString()) initial = item;
    }
    content->addWidget(table_, 3);
    auto *editColumn = new QVBoxLayout;
    editColumn->setSpacing(12);
    auto *editor = new QGroupBox(uiText(u"边界条件"), this);
    auto *form = new QFormLayout(editor);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setVerticalSpacing(10);
    name_ = new QLineEdit(editor);
    name_->setObjectName(QStringLiteral("boundaryName"));
    name_->setMaxLength(128);
    type_ = new QComboBox(editor);
    type_->setObjectName(QStringLiteral("boundaryType"));
    for (const QString &type : {QStringLiteral("symmetry"), QStringLiteral("inviscid-wall"), QStringLiteral("farfield")})
        type_->addItem(typeLabel(type), type);
    form->addRow(uiText(u"名称"), name_);
    form->addRow(uiText(u"边界类型"), type_);
    editColumn->addWidget(editor);
    auto *footer = new QHBoxLayout;
    feedback_ = new QLabel(this);
    feedback_->setObjectName(QStringLiteral("boundaryFeedback"));
    feedback_->setWordWrap(true);
    feedback_->setStyleSheet(QStringLiteral("color: #52677f;"));
    footer->addWidget(feedback_, 1);
    apply_ = new QPushButton(uiText(u"应用"), this);
    apply_->setObjectName(QStringLiteral("applyBoundary"));
    footer->addWidget(apply_);
    editColumn->addLayout(footer);
    editColumn->addStretch();
    content->addLayout(editColumn, 2);
    layout->addLayout(content);
    connect(table_, &QTreeWidget::currentItemChanged, this, [this] { loadSelection(); });
    connect(name_, &QLineEdit::textChanged, this, [this] { validateEdit(); });
    connect(type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { validateEdit(); });
    connect(apply_, &QPushButton::clicked, this, &BoundaryConditionsPanel::applyEdit);
    if (!initial) initial = table_->topLevelItem(0);
    table_->setCurrentItem(initial);
    if (!initial) loadSelection();
    // Keep screenshot order initially; header clicks can sort without changing row identity.
    table_->header()->setSortIndicator(-1, Qt::AscendingOrder);
    table_->setSortingEnabled(true);
}

QString BoundaryConditionsPanel::typeLabel(const QString &type) const
{
    if (type == "symmetry") return uiText(u"对称面");
    if (type == "inviscid-wall") return uiText(u"无粘壁面");
    if (type == "farfield") return uiText(u"远场");
    return type;
}

void BoundaryConditionsPanel::loadSelection()
{
    const QSignalBlocker blockName(name_);
    const QSignalBlocker blockType(type_);
    auto *item = table_->currentItem();
    name_->setEnabled(item != nullptr);
    type_->setEnabled(item != nullptr);
    name_->setText(item ? item->text(0) : QString());
    type_->setCurrentIndex(item ? type_->findData(item->data(1, Qt::UserRole)) : -1);
    validateEdit();
}

void BoundaryConditionsPanel::validateEdit()
{
    auto *item = table_->currentItem();
    QString message;
    const QString name = name_->text().trimmed();
    if (!item) message = uiText(u"请选择边界。");
    else if (name.isEmpty()) message = uiText(u"边界名称不能为空。");
    else {
        for (int i = 0; i < table_->topLevelItemCount(); ++i) {
            auto *other = table_->topLevelItem(i);
            if (other != item && other->text(2) == item->text(2) && other->text(0) == name) {
                message = uiText(u"同一网格中的边界名称不能重复。");
                break;
            }
        }
    }
    const bool changed = item && (name != item->text(0) || type_->currentData() != item->data(1, Qt::UserRole));
    apply_->setEnabled(message.isEmpty() && changed && type_->currentIndex() >= 0);
    feedback_->setText(!message.isEmpty() ? message : changed ? uiText(u"修改尚未应用；切换边界将放弃未应用的修改。") : uiText(u"选择边界后编辑名称和类型。"));
}

void BoundaryConditionsPanel::applyEdit()
{
    validateEdit();
    if (!apply_->isEnabled()) return;
    auto *item = table_->currentItem();
    const QString id = item->data(0, Qt::UserRole).toString();
    auto rows = draft_.value("boundaries").toList();
    for (auto &value : rows) {
        auto row = value.toMap();
        if (row.value("id").toString() != id) continue;
        row.insert("name", name_->text().trimmed());
        row.insert("type", type_->currentData());
        value = row;
        item->setText(0, row.value("name").toString());
        item->setData(1, Qt::UserRole, row.value("type"));
        item->setText(1, typeLabel(row.value("type").toString()));
        break;
    }
    draft_.insert("boundaries", rows);
    draft_.insert("selectedId", id);
    emit draftChanged(draft_);
    loadSelection();
    feedback_->setText(uiText(u"已应用到当前窗口草稿。"));
}

} // namespace aerosim
