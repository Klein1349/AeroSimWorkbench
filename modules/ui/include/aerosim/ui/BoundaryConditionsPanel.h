#pragma once

#include <QVariantMap>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace aerosim {

// UI draft for the reference M6 boundaries, pending real mesh service integration.
class BoundaryConditionsPanel final : public QWidget {
    Q_OBJECT
public:
    explicit BoundaryConditionsPanel(const QVariantMap &draft, QWidget *parent = nullptr);

signals:
    void draftChanged(const QVariantMap &draft);

private:
    void loadSelection();
    void validateEdit();
    void applyEdit();
    QString typeLabel(const QString &type) const;
    QVariantMap draft_;
    QTreeWidget *table_ = nullptr;
    QLineEdit *name_ = nullptr;
    QComboBox *type_ = nullptr;
    QLabel *feedback_ = nullptr;
    QPushButton *apply_ = nullptr;
};

} // namespace aerosim
