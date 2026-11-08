#pragma once

#include <QColor>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QSplitter;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace aerosim {

// Front-end-only mesh workspace. It owns no mesh data or VTK objects; the
// actual viewer remains supplied through MainWindow's Viewport widget slot.
class MeshManagementPage final : public QWidget {
    Q_OBJECT
public:
    explicit MeshManagementPage(QWidget *viewportHost, QWidget *parent = nullptr);
    QVariantMap layoutState() const;
    void restoreLayoutState(const QVariantMap &state);
    void restoreDefaultLayout();
    void enableBoundaryMode();
    void beginBoundary(const QString &id, const QString &path);
    void finishBoundary(const QString &id, const QString &summary);
    void failBoundary(const QString &id, const QString &message);
    void setBoundaryBusy(bool busy);
signals:
    void boundaryImportRequested(const QString &path);
    void boundaryRemoveRequested(const QString &id);
    void boundaryStyleChanged(const QString &id, const QVariantMap &style);
    void boundaryViewRequested(const QString &command);
    void boundaryBackgroundRequested(int mode);

private:
    struct MeshUiState {
        QString id;
        QString summary;
        bool ready = true;
        QString name;
        bool visible = true;
        QString displayMode;
        QColor color;
        int opacity = 100;
        double scale = 1.0;
        QString unit;
    };

    void addMesh();
    void removeCurrentMesh();
    void selectMesh(int row);
    void refreshListItem(int row);
    void refreshEditor();
    void updateCurrentMesh();
    void updateCount();
    void appendHistory(const QString &message);
    void setHistoryCollapsed(bool collapsed);
    void emitBoundaryStyle(int row);
    bool boundaryMode_ = false;
    bool boundaryBusy_ = false;

    QVector<MeshUiState> meshes_;
    QTreeWidget *meshList_ = nullptr;
    QLabel *meshCount_ = nullptr;
    QPushButton *addMeshButton_ = nullptr;
    QPushButton *removeMeshButton_ = nullptr;
    QLineEdit *nameEditor_ = nullptr;
    QCheckBox *visibleEditor_ = nullptr;
    QComboBox *displayModeEditor_ = nullptr;
    QPushButton *colorButton_ = nullptr;
    QSlider *opacityEditor_ = nullptr;
    QLabel *opacityValue_ = nullptr;
    QDoubleSpinBox *scaleEditor_ = nullptr;
    QComboBox *unitEditor_ = nullptr;
    QLabel *viewerStatus_ = nullptr;
    QLabel *axisIndicator_ = nullptr;
    QSplitter *mainSplitter_ = nullptr;
    QSplitter *verticalSplitter_ = nullptr;
    QTabWidget *historyTabs_ = nullptr;
    QListWidget *historyList_ = nullptr;
    QPushButton *historyCollapseButton_ = nullptr;
    int expandedHistoryHeight_ = 145;
    bool historyCollapsed_ = false;
    bool updatingEditor_ = false;
};

} // namespace aerosim
