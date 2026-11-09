#include "aerosim/ui/MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QComboBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QDialog>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace {
QString text(const char16_t *value)
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value));
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    aerosim::MainWindow window;
    window.show();
    app.processEvents();
    const auto check = [](bool condition, const char *message) {
        if (!condition) QTextStream(stderr) << message << '\n';
        return condition;
    };

    auto *tree = window.findChild<QTreeWidget *>("workspaceTree");
    auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
    auto *property = window.findChild<QDockWidget *>("propertyDock");
    if (!check(tree && workspace && property, "Main calculation workbench controls are missing")) return 1;
    if (!check(tree->topLevelItemCount() == 1, "Workspace navigation must have one root")) return 2;
    auto *root = tree->topLevelItem(0);
    auto *calculation = root->child(2);
    if (!check(calculation && calculation->text(0) == text(u"\u8ba1\u7b97\u4efb\u52a1")
               && calculation->childCount() == 2,
               "Calculation navigation must expose exactly two task pages")) return 3;
    if (!check(calculation->child(0)->text(0) == text(u"\u5355\u4efb\u52a1")
               && calculation->child(1)->text(0) == text(u"\u591a\u4efb\u52a1")
               && calculation->child(0)->childCount() == 0
               && calculation->child(1)->childCount() == 0,
               "Single and multiple task pages must be leaf nodes")) return 4;

    tree->setCurrentItem(calculation->child(0));
    app.processEvents();
    auto *single = window.findChild<QWidget *>("singleTaskPage");
    if (!check(workspace->currentWidget()->objectName() == "singleTaskPage"
               && single && property->isHidden()
               && single->findChild<QCheckBox *>("singleTaskResume")
               && single->findChild<QPushButton *>("submitCalculation")
               && single->findChild<QPushButton *>("calculationConditionTab")
               && single->findChild<QPlainTextEdit *>("calculationLog"),
               "Single task reference layout is incomplete")) return 5;
    if (!check(single->findChild<QPushButton *>("calculationConditionTab")->text() == text(u"\u5355\u5de5\u51b5")
               && !single->findChild<QPushButton *>("calculationMonitorTab"),
               "Single task must expose only the single-condition view button")) return 6;
    const QString captureDir = qEnvironmentVariable("AEROSIM_CALCULATION_CAPTURE_DIR");
    if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(QStringLiteral("single-task.png")));
    if (!check(!window.findChild<QDockWidget *>("outputDock")->isVisible(),
               "Calculation pages must keep the log inside the right-hand page")) return 7;

    tree->setCurrentItem(calculation->child(1));
    app.processEvents();
    auto *multi = window.findChild<QWidget *>("multipleTaskPage");
    auto *table = window.findChild<QTableWidget *>("multipleTaskTable");
    if (!check(workspace->currentWidget()->objectName() == "multipleTaskPage"
               && multi && table && table->columnCount() == 8
               && property->isHidden()
               && multi->findChild<QPushButton *>("calculationConditionTab")->text() == text(u"\u591a\u5de5\u51b5")
               && multi->findChild<QPushButton *>("calculationMonitorTab"),
               "Multiple task reference layout is incomplete")) return 8;
    int categoryButtons = 0;
    for (auto *button : multi->findChildren<QPushButton *>())
        categoryButtons += button->property("taskCategory").toBool();
    if (!check(categoryButtons == 5
               && window.findChild<QPushButton *>("addTask")
               && window.findChild<QPushButton *>("removeSelectedTasks")
               && window.findChild<QPushButton *>("clearTasks"),
               "Multiple task category controls are incomplete")) return 9;
    if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(QStringLiteral("multiple-task.png")));
    if (!check(!window.findChild<QComboBox *>("multipleTaskCategory")
               && window.findChild<QPlainTextEdit *>("calculationLog"),
               "Task category input must use buttons and retain the right-hand log")) return 10;

    QTimer::singleShot(0, &window, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog) return;
        const auto edits = dialog->findChildren<QLineEdit *>();
        const QStringList values = {QStringLiteral("100000"), QStringLiteral("300"),
                                    QStringLiteral("0.8"), QStringLiteral("2"), QStringLiteral("0")};
        for (int i = 0; i < edits.size() && i < values.size(); ++i) edits[i]->setText(values[i]);
        if (auto *buttons = dialog->findChild<QDialogButtonBox *>())
            buttons->button(QDialogButtonBox::Ok)->click();
    });
    multi->findChild<QPushButton *>("addTaskCategory_0")->click();
    app.processEvents();
    if (!check(table->rowCount() == 1, "A category button must add one configured task row")) return 11;
    table->item(0, 0)->setCheckState(Qt::Checked);
    multi->findChild<QPushButton *>("removeSelectedTasks")->click();
    if (!check(table->rowCount() == 0, "Removing the selected task must remove its row")) return 12;

    QTextStream(stdout) << "Calculation task reference UI checks passed\n";
    return 0;
}
