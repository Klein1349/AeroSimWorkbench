#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/MeshManagementPage.h"

#include <QApplication>
#include <QAction>
#include <QColorDialog>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QRadioButton>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolBar>
#include <QCheckBox>
#include <QDockWidget>
#include <QComboBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QDialog>
#include <QGroupBox>
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
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QTemporaryDir preferences;
    if (!preferences.isValid()) return 22;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, preferences.path());
    aerosim::MainWindow window;
    window.show();
    app.processEvents();
    const auto check = [](bool condition, const char *message) {
        if (!condition) QTextStream(stderr) << message << '\n';
        return condition;
    };

    {
        aerosim::MeshManagementPage page(new QWidget);
        page.enableBoundaryMode();
        auto *toolbar = page.findChild<QToolBar *>("meshViewerToolbar");
        if (!check(toolbar && toolbar->isMovable() && toolbar->allowedAreas() == Qt::AllToolBarAreas,
                   "Mesh toolbar must allow four-edge docking")) return 94;
        QVector<QImage> glyphs;
        for (auto *action : toolbar->actions()) {
            if (action->isSeparator()) continue;
            const QImage small = action->icon().pixmap(QSize(22, 22)).toImage();
            const QImage large = action->icon().pixmap(QSize(64, 64)).toImage();
            const QImage disabled = action->icon().pixmap(QSize(22, 22), QIcon::Disabled).toImage();
            auto *button = toolbar->widgetForAction(action);
            if (!check(button && (button->focusPolicy() & Qt::TabFocus) && !button->accessibleName().isEmpty()
                       && !action->toolTip().isEmpty() && !small.isNull() && large.width() > small.width()
                       && small.pixelColor(0, 0).alpha() == 0 && small != disabled && !glyphs.contains(small),
                       "Each toolbar action needs a distinct scalable icon, disabled state and accessible label")) return 114;
            glyphs.append(small);
        }
        const auto zoomInIndex = toolbar->actions().indexOf(toolbar->findChild<QAction *>("boundaryView_in"));
        const auto zoomOutIndex = toolbar->actions().indexOf(toolbar->findChild<QAction *>("boundaryView_out"));
        if (!check(glyphs.size() == 17 && zoomInIndex >= 0 && zoomOutIndex == zoomInIndex + 1,
                   "The zoom controls must be adjacent in either docking orientation")) return 115;
        for (const auto area : {Qt::LeftToolBarArea, Qt::TopToolBarArea,
                                Qt::BottomToolBarArea, Qt::RightToolBarArea}) {
            page.restoreLayoutState({{QStringLiteral("meshToolbarArea"), int(area)}});
            const auto orientation = (area == Qt::LeftToolBarArea || area == Qt::RightToolBarArea)
                ? Qt::Vertical : Qt::Horizontal;
            if (!check(page.layoutState().value(QStringLiteral("meshToolbarArea")).toInt() == int(area)
                       && toolbar->orientation() == orientation,
                       "Restoring a mesh toolbar edge must update its orientation")) return 95;
        }
        auto *axis = page.findChild<QWidget *>("axisIndicator");
        if (!check(axis && !axis->testAttribute(Qt::WA_TransparentForMouseEvents),
                   "The orientation control must receive mouse events")) return 91;
        QString view;
        QPoint drag;
        QObject::connect(&page, &aerosim::MeshManagementPage::boundaryViewRequested,
                         &page, [&view](const QString &command) { view = command; });
        QObject::connect(&page, &aerosim::MeshManagementPage::boundaryOrbitRequested,
                         &page, [&drag](int dx, int dy) { drag += QPoint(dx, dy); });
        const auto mouse = [axis](QEvent::Type type, QPoint position,
                                  Qt::MouseButton button, Qt::MouseButtons buttons) {
            QMouseEvent event(type, QPointF(position), button, buttons, Qt::NoModifier);
            QApplication::sendEvent(axis, &event);
        };
        mouse(QEvent::MouseButtonPress, QPoint(77, 44), Qt::LeftButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, QPoint(77, 44), Qt::LeftButton, Qt::NoButton);
        if (!check(view == QStringLiteral("right") && drag.isNull(),
                   "Clicking the positive X endpoint must request the right view")) return 92;

        view.clear();
        mouse(QEvent::MouseButtonPress, QPoint(44, 44), Qt::LeftButton, Qt::LeftButton);
        const int offset = QApplication::startDragDistance() + 5;
        mouse(QEvent::MouseMove, QPoint(44 + offset, 46), Qt::NoButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, QPoint(44 + offset, 46), Qt::LeftButton, Qt::NoButton);
        if (!check(view.isEmpty() && drag == QPoint(offset, 2),
                   "Dragging the orientation control must orbit without snapping")) return 93;

        auto *edges = toolbar->findChild<QAction *>("meshSurfaceEdgesButton");
        auto *surface = toolbar->findChild<QAction *>("meshSurfaceButton");
        auto *wire = toolbar->findChild<QAction *>("meshWireframeButton");
        auto *center = toolbar->findChild<QAction *>("boundaryView_center");
        auto *fit = toolbar->findChild<QAction *>("boundaryView_fit");
        if (!check(center && fit && !center->isEnabled() && !fit->isEnabled(),
                   "Center and fit need a visible boundary")) return 116;
        auto *meshList = page.findChild<QTableWidget *>("meshTable");
        auto *status = page.findChild<QLabel *>("meshViewerStatus");
        auto *removeMesh = page.findChild<QPushButton *>("deleteMeshButton");
        auto *properties = page.findChild<QGroupBox *>("meshDisplayProperties");
        auto *name = page.findChild<QLineEdit *>("meshNameEditor");
        auto *visible = page.findChild<QCheckBox *>("meshVisibleEditor");
        auto *color = page.findChild<QPushButton *>("meshColorButton");
        auto *opacity = page.findChild<QSlider *>("meshOpacityEditor");
        auto *mode = page.findChild<QComboBox *>("meshDisplayModeEditor");
        auto *unit = page.findChild<QComboBox *>("meshUnitEditor");
        if (!check(edges && surface && wire && meshList && status && removeMesh && properties
                   && name && visible && color && opacity && mode && unit,
                   "Missing per-file mesh properties or viewer controls")) return 96;

        struct Appearance {
            QColor color;
            int opacity = -1;
            int mode = -1;
        };
        QHash<QString, Appearance> rendered;
        QStringList appearanceEdits;
        QObject::connect(&page, &aerosim::MeshManagementPage::boundaryAppearanceChanged,
                         &page, [&](const QString &id, const QColor &tint, int alpha, int displayMode) {
            rendered.insert(id, {tint, alpha, displayMode});
            appearanceEdits.append(id);
        });
        const auto finish = [&](const QString &id, const QString &summary, qint64 faces) {
            const int initialMode = page.finishBoundary(id, summary, faces);
            rendered.insert(id, {QColor(color->text()), opacity->value(), initialMode});
        };
        page.beginBoundary(QStringLiteral("cube"), QStringLiteral("Cube.vtk"));
        if (!check(!center->isEnabled() && !fit->isEnabled(),
                   "A loading row has no bounds for center or fit")) return 117;
        finish(QStringLiteral("cube"), QStringLiteral("Cube summary"), 9999999);
        if (!check(edges->isChecked() && mode->currentIndex() == 2 && rendered.value("cube").mode == 2,
                   "Smaller imports should initially show surface and edges")) return 106;
        const QColor cubeColor = rendered.value("cube").color;
        page.beginBoundary(QStringLiteral("cylinder"), QStringLiteral("Cylinder.vtu"));
        if (!check(!mode->isEnabled() && center->isEnabled() && fit->isEnabled(),
                   "A pending selection must leave earlier visible bounds available")) return 107;
        finish(QStringLiteral("cylinder"), QStringLiteral("Cylinder summary"), 10000000);
        if (!check(surface->isChecked() && mode->currentIndex() == 0
                   && rendered.value("cylinder").mode == 0 && rendered.value("cube").mode == 2,
                   "At ten million faces only the new import should default to surface")) return 108;
        appearanceEdits.clear();
        wire->trigger();
        if (!check(mode->currentIndex() == 1 && appearanceEdits == QStringList{QStringLiteral("cylinder")}
                   && rendered.value("cylinder").mode == 1 && rendered.value("cube").mode == 2
                   && wire->shortcut() == QKeySequence(Qt::Key_W)
                   && surface->shortcut() == QKeySequence(Qt::Key_S),
                   "The viewport representation actions must target the selected mesh")) return 105;
        page.beginBoundary(QStringLiteral("sphere"), QStringLiteral("Sphere.vtp"));
        finish(QStringLiteral("sphere"), QStringLiteral("Sphere summary"), 20000000);
        if (!check(surface->isChecked() && rendered.value("sphere").mode == 0
                   && rendered.value("cylinder").mode == 1 && rendered.value("cube").mode == 2,
                   "Later imports must preserve the representation of every previous mesh")) return 109;

        appearanceEdits.clear();
        properties->setChecked(false);
        page.selectBoundary(QStringLiteral("cylinder"));
        if (!check(properties->isChecked() && opacity->isVisibleTo(&page) && opacity->isEnabled()
                   && meshList->currentRow() == 1 && name->text() == QStringLiteral("Cylinder.vtu")
                   && status->text().contains(QStringLiteral("Cylinder summary")) && wire->isChecked()
                   && appearanceEdits.isEmpty(),
                   "Picking a mesh in 3D must open its own properties without restyling it")) return 98;
        if (!check(page.metaObject()->indexOfSignal("boundaryAxisRotationRequested(QString,QVector3D)") < 0
                   && page.metaObject()->indexOfSignal("boundaryDragRotationRequested(QString,int,int)") < 0
                   && !page.findChild<QWidget *>("meshScaleEditor"),
                   "Mesh placement must stay fixed in file coordinates")) return 102;

        page.setAttribute(Qt::WA_DontShowOnScreen);
        page.resize(1050, 780);
        page.show();
        app.processEvents();
        const auto clickRow = [&](int row) {
            const QPoint pos = meshList->visualItemRect(meshList->item(row, 1)).center();
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(pos), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(meshList->viewport(), &press);
            QMouseEvent release(QEvent::MouseButtonRelease, QPointF(pos), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(meshList->viewport(), &release);
        };
        properties->setChecked(false);
        clickRow(1);
        if (!check(properties->isChecked() && unit->isVisibleTo(&page) && name->isEnabled(),
                   "A single click on the current table row must reopen its properties")) return 110;

        name->setText(QStringLiteral("Outer wall"));
        QMetaObject::invokeMethod(name, "editingFinished", Qt::DirectConnection);
        unit->setCurrentText(QStringLiteral("mm"));
        if (!check(meshList->item(1, 1)->text() == QStringLiteral("Outer wall")
                   && meshList->item(1, 1)->data(Qt::UserRole).toString() == QStringLiteral("cylinder")
                   && appearanceEdits.isEmpty(),
                   "Name and unit are metadata and must retain the original boundary ID")) return 111;
        const QColor wallColor(19, 114, 151);
        QTimer::singleShot(0, &page, [&] {
            if (auto *picker = page.findChild<QColorDialog *>()) {
                picker->setCurrentColor(wallColor);
                picker->accept();
            }
        });
        color->click();
        mode->setCurrentIndex(2);
        opacity->setValue(35);
        QStringList hiddenIds;
        QObject::connect(&page, &aerosim::MeshManagementPage::boundaryVisibilityChanged,
                         &page, [&](const QString &id, bool visible) { if (!visible) hiddenIds.append(id); });
        visible->click();
        if (!check(rendered.value("cylinder").color == wallColor && rendered.value("cylinder").mode == 2
                   && rendered.value("cylinder").opacity == 35 && hiddenIds == QStringList{QStringLiteral("cylinder")}
                   && meshList->item(1, 2)->checkState() == Qt::Unchecked,
                   "Edits must change only the chosen boundary and synchronize table visibility")) return 104;

        properties->setChecked(false);
        appearanceEdits.clear();
        clickRow(0);
        if (!check(properties->isChecked() && name->text() == QStringLiteral("Cube.vtk")
                   && unit->currentText() == QStringLiteral("m") && visible->isChecked()
                   && opacity->value() == 100 && mode->currentIndex() == 2
                   && rendered.value("cube").color == cubeColor && appearanceEdits.isEmpty(),
                   "Clicking another row must restore its independent settings")) return 103;
        surface->trigger();
        meshList->item(0, 2)->setCheckState(Qt::Unchecked);
        if (!check(!visible->isChecked() && hiddenIds == QStringList{QStringLiteral("cylinder"), QStringLiteral("cube")}
                   && appearanceEdits == QStringList{QStringLiteral("cube")} && rendered.value("cube").mode == 0,
                   "The table checkbox and toolbar must affect their own boundary")) return 112;
        page.selectBoundary(QStringLiteral("cylinder"));
        if (!check(name->text() == QStringLiteral("Outer wall") && unit->currentText() == QStringLiteral("mm")
                   && !visible->isChecked() && opacity->value() == 35 && mode->currentIndex() == 2
                   && color->text().compare(wallColor.name(), Qt::CaseInsensitive) == 0,
                   "Reselecting a file must restore every edited display property")) return 113;

        view.clear();
        drag = QPoint();
        mouse(QEvent::MouseButtonPress, QPoint(77, 44), Qt::LeftButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, QPoint(77, 44), Qt::LeftButton, Qt::NoButton);
        if (!check(view == QStringLiteral("right") && drag.isNull(),
                   "With a mesh selected the X endpoint must still move the camera")) return 99;
        view.clear();
        mouse(QEvent::MouseButtonPress, QPoint(44, 44), Qt::LeftButton, Qt::LeftButton);
        page.selectBoundary(QStringLiteral("sphere"));
        mouse(QEvent::MouseMove, QPoint(44 + offset, 46), Qt::NoButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, QPoint(44 + offset, 46), Qt::LeftButton, Qt::NoButton);
        if (!check(view.isEmpty() && drag == QPoint(offset, 2),
                   "Changing mesh selection during a drag must preserve the camera orbit")) return 100;

        meshList->setCurrentCell(0, 1);
        removeMesh->click();
        if (!check(meshList->rowCount() == 2 && meshList->currentRow() == 0
                   && meshList->item(0, 1)->text() == QStringLiteral("Outer wall") && opacity->value() == 35
                   && status->text().contains(QStringLiteral("Cylinder summary")),
                   "Removing a mesh must show the selected remaining mesh information")) return 97;
        page.selectBoundary(QString());
        if (!check(meshList->currentRow() == -1 && name->text().isEmpty() && !opacity->isEnabled()
                   && !removeMesh->isEnabled() && !wire->isEnabled() && center->isEnabled() && fit->isEnabled(),
                   "Clearing selection must disable per-object edits")) return 101;
        meshList->item(1, 2)->setCheckState(Qt::Unchecked);
        if (!check(!center->isEnabled() && !fit->isEnabled(),
                   "Hiding the last visible boundary without a selection must disable center and fit")) return 118;
        page.selectBoundary(QStringLiteral("sphere"));
        visible->click();
        if (!check(center->isEnabled() && fit->isEnabled(),
                   "Showing a boundary in the property editor must immediately enable center and fit")) return 119;
        removeMesh->click();
        if (!check(!center->isEnabled() && !fit->isEnabled(),
                   "Removing the last visible boundary must disable center and fit")) return 120;
    }

    auto *meshPage = window.findChild<QWidget *>("viewportPage");
    auto *meshTable = window.findChild<QTableWidget *>("meshTable");
    auto *meshCount = window.findChild<QLabel *>("meshObjectCount");
    auto *importMesh = window.findChild<QPushButton *>("importMeshButton");
    auto *deleteMesh = window.findChild<QPushButton *>("deleteMeshButton");
    auto *historyTabs = window.findChild<QTabWidget *>("meshHistoryTabs");
    auto *history = window.findChild<QListWidget *>("meshOperationHistory");
    auto *historyCollapse = window.findChild<QPushButton *>("meshHistoryCollapseButton");
    auto *workspaceTabs = window.findChild<QTabWidget *>("meshWorkspaceTabs");
    if (!check(meshPage && meshTable && meshTable->rowCount() == 3
               && meshTable->columnCount() == 3
               && meshCount && meshCount->text().contains("3 / 10")
               && importMesh && deleteMesh && historyTabs && history && historyCollapse
               && meshPage->findChild<QGroupBox *>("meshDisplayProperties")
               && !meshPage->findChild<QWidget *>("meshScaleEditor")
               && workspaceTabs && workspaceTabs->count() == 1
               && workspaceTabs->tabText(0) == text(u"\u7f51\u683c\u89c6\u56fe")
               && window.findChild<QSplitter *>("meshWorkspaceMainSplitter")
               && window.findChild<QSplitter *>("meshSettingsSplitter")
               && window.findChild<QWidget *>("viewportSlot")
               && window.findChild<QDockWidget *>("propertyDock")->isHidden()
               && window.findChild<QWidget *>("outputBody")->isHidden()
               && historyTabs->isHidden()
               && historyCollapse->text() == text(u"\u5c55\u5f00")
               && !window.findChild<QPlainTextEdit *>("meshScriptPlaceholder")->isEnabled(),
               "Mesh management workspace is incomplete")) return 57;
    // The replicated reference layout exposes a coordinate system selector and
    // a 序号/网格名称/是否可见 table next to the tabbed workspace.
    auto *solidRadio = window.findChild<QRadioButton *>("meshCoordinateSystemSolid");
    auto *planarRadio = window.findChild<QRadioButton *>("meshCoordinateSystemPlanar");
    if (!check(solidRadio && planarRadio && solidRadio->isChecked()
               && meshTable->horizontalHeaderItem(0)
               && meshTable->horizontalHeaderItem(0)->text() == text(u"\u5e8f\u53f7")
               && meshTable->horizontalHeaderItem(1)->text() == text(u"\u7f51\u683c\u540d\u79f0")
               && meshTable->horizontalHeaderItem(2)->text() == text(u"\u662f\u5426\u53ef\u89c1"),
               "Mesh information table does not match the reference layout")) return 86;
    // Initial visibility must mirror the seeded state (the first two meshes are
    // visible, the third is hidden). Assigning a checkable cell emits
    // itemChanged before its state is set, which used to be read back as the
    // user hiding every mesh.
    if (!check(meshTable->item(0, 2)->checkState() == Qt::Checked
               && meshTable->item(1, 2)->checkState() == Qt::Checked
               && meshTable->item(2, 2)->checkState() == Qt::Unchecked,
               "Initial mesh visibility was not preserved in the table")) return 87;
    if (!check(!history->count() || !history->item(history->count() - 1)->text()
                   .contains(text(u"\u9690\u85c5 Mesh_001")),
               "Seeding the table must not log a spurious hide")) return 88;
    meshTable->setCurrentCell(1, 1);
    meshTable->item(1, 2)->setCheckState(Qt::Unchecked);
    meshTable->setCurrentCell(0, 1);
    meshTable->setCurrentCell(1, 1);
    app.processEvents();
    if (!check(meshTable->item(0, 2)->checkState() == Qt::Checked
               && meshTable->item(1, 2)->checkState() == Qt::Unchecked
               && history->count() >= 1,
               "Mock visibility must survive row changes")) return 58;
    for (int i = meshTable->rowCount(); i < 10; ++i) importMesh->click();
    if (!check(meshTable->rowCount() == 10 && !importMesh->isEnabled()
               && meshCount->text().contains("10 / 10"),
               "Mesh management did not enforce its ten-object UI limit")) return 59;
    deleteMesh->click();
    if (!check(meshTable->rowCount() == 9 && importMesh->isEnabled(),
               "Removing a mock mesh did not update list availability")) return 60;
    historyCollapse->click();
    app.processEvents();
    if (!check(!historyTabs->isHidden() && historyCollapse->text() == text(u"\u6536\u8d77"),
               "Mesh operation history did not expand")) return 61;
    historyCollapse->click();
    app.processEvents();
    if (!check(historyTabs->isHidden() && historyCollapse->text() == text(u"\u5c55\u5f00"),
               "Mesh operation history did not collapse")) return 62;

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
               && single->findChild<QComboBox *>("submissionModeCombo")
               && single->findChildren<QGroupBox *>("calculationObservables").size() == 1
               && single->findChild<QPushButton *>("calculationConditionTab")
               && single->findChild<QPlainTextEdit *>("calculationRunHistory")
               && single->findChild<QPlainTextEdit *>("calculationSolverOutput")
               && single->findChild<QCheckBox *>("singleTaskResume"),
               "Single task reference layout is incomplete")) return 5;
    if (!check(single->findChild<QPushButton *>("calculationConditionTab")->text() == text(u"\u5355\u5de5\u51b5")
               && !single->findChild<QPushButton *>("calculationMonitorTab"),
               "Single task must expose only the single-condition view button")) return 6;
    const QString captureDir = qEnvironmentVariable("AEROSIM_CALCULATION_CAPTURE_DIR");
    if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(QStringLiteral("single-task.png")));
    if (!check(!window.findChild<QDockWidget *>("outputDock")->isVisible(),
               "Calculation pages must keep the log inside the right-hand page")) return 7;
    single->findChild<QPushButton *>("submitCalculation")->click();
    app.processEvents();
    if (!check(single->findChild<QPlainTextEdit *>("calculationRunHistory")->toPlainText().contains(text(u"已提交计算")),
               "Submitting a single task must record a submitted message")) return 8;

    tree->setCurrentItem(calculation->child(1));
    app.processEvents();
    auto *multi = window.findChild<QWidget *>("multipleTaskPage");
    auto *table = window.findChild<QTableWidget *>("multipleTaskTable");
    if (!check(workspace->currentWidget()->objectName() == "multipleTaskPage"
               && multi && table && table->columnCount() == 8
               && property->isHidden()
               && multi->findChild<QPushButton *>("calculationConditionTab")->text() == text(u"\u591a\u5de5\u51b5")
               && !multi->findChild<QPushButton *>("calculationMonitorTab"),
               "Multiple task reference layout is incomplete")) return 9;
    int categoryButtons = 0;
    for (auto *button : multi->findChildren<QPushButton *>())
        categoryButtons += button->property("taskCategory").toBool();
    if (!check(categoryButtons == 5
               && multi->findChild<QGroupBox *>("taskCategoryGroup")
               && window.findChild<QPushButton *>("removeSelectedTasks")
               && !window.findChild<QPushButton *>("addTask")
               && !window.findChild<QPushButton *>("clearTasks"),
               "Multiple task category controls are incomplete")) return 10;
    if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(QStringLiteral("multiple-task.png")));
    if (!check(!window.findChild<QComboBox *>("multipleTaskCategory")
               && multi->findChild<QPlainTextEdit *>("calculationRunHistory")
               && multi->findChild<QPlainTextEdit *>("calculationSolverOutput")
               && multi->findChild<QPushButton *>("submitCalculation")
               && multi->findChild<QComboBox *>("submissionModeCombo")
               && multi->findChild<QComboBox *>("submissionModeCombo")->count() == 3,
               "Task category input must use buttons and retain the right-hand log")) return 11;

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
    if (!check(table->rowCount() == 1, "A category button must add one configured task row")) return 12;
    if (!check(multi->findChild<QPlainTextEdit *>("calculationRunHistory")->toPlainText().contains(text(u"\u5df2\u6dfb\u52a0")),
               "Adding a task must be recorded in run history")) return 13;
    auto *selectAll = multi->findChild<QCheckBox *>("selectAllTasks");
    auto *remove = multi->findChild<QPushButton *>("removeSelectedTasks");
    table->selectRow(0);
    app.processEvents();
    if (!check(!selectAll->isChecked() && !remove->isEnabled(),
               "Highlighting a row must not select it for deletion")) return 14;
    remove->click();
    if (!check(table->rowCount() == 1, "Delete must ignore a highlighted but unchecked row")) return 15;
    table->item(0, 0)->setCheckState(Qt::Checked);
    app.processEvents();
    if (!check(selectAll->isChecked() && remove->isEnabled(),
               "The select-all indicator must reflect every sequence checkbox")) return 16;
    multi->findChild<QPushButton *>("submitCalculation")->click();
    if (!check(multi->findChild<QPlainTextEdit *>("calculationRunHistory")->toPlainText().contains(text(u"已提交计算")),
               "Submitting multiple tasks must record a submitted message")) return 17;
    remove->click();
    if (!check(table->rowCount() == 0, "Removing the checked task must remove its row")) return 18;

    QTextStream(stdout) << "Calculation task reference UI checks passed\n";
    return 0;
}
