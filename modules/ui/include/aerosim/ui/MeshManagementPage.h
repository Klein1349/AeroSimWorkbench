#pragma once

#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QVector3D>
#include <QWidget>

class QAction;
class QLabel;
class QListWidget;
class QMainWindow;
class QPushButton;
class QRadioButton;
class QSplitter;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;
class QToolBar;

namespace aerosim {

// Front-end-only mesh workspace. It owns no mesh data or VTK objects; the
// actual viewer remains supplied through MainWindow's Viewport widget slot.
// The page mirrors the reference mesh manager: a "网格信息" settings panel
// (coordinate system, mesh table, import/delete) sits left of the tabbed
// workspace that carries the persistent VTK viewport.
class MeshManagementPage final : public QWidget {
    Q_OBJECT
public:
    explicit MeshManagementPage(QWidget *viewportHost, QWidget *parent = nullptr);
    QVariantMap layoutState() const;
    void restoreLayoutState(const QVariantMap &state);
    void restoreDefaultLayout();
    void enableBoundaryMode(bool cgnsAvailable = false);
    void beginBoundary(const QString &id, const QString &path);
    void finishBoundary(const QString &id, const QString &summary, qint64 boundaryFaces = 0);
    void failBoundary(const QString &id, const QString &message);
    void setBoundaryBusy(bool busy);
    void setBoundaryCameraOrientation(const QVector3D &from, const QVector3D &up);
    void selectBoundary(const QString &id);
signals:
    void boundaryImportRequested(const QStringList &paths);
    void boundaryRemoveRequested(const QString &id);
    void boundaryVisibilityChanged(const QString &id, bool visible);
    void boundaryDisplayModeChanged(int mode);
    void boundaryViewRequested(const QString &command);
    void boundaryOrbitRequested(int dx, int dy);
    void boundaryBackgroundRequested(int mode);

private:
    struct MeshUiState {
        QString id;
        QString summary;
        bool ready = true;
        QString name;
        bool visible = true;
    };

    void addMesh();
    void removeCurrentMesh();
    void selectMesh(int row);
    void refreshTableRow(int row);
    void refreshSelection();
    void updateCount();
    void appendHistory(const QString &message);
    void setHistoryCollapsed(bool collapsed);
    int selectedRow() const;
    bool boundaryMode_ = false;
    bool boundaryBusy_ = false;
    bool cgnsAvailable_ = false;

    QVector<MeshUiState> meshes_;
    QTableWidget *meshTable_ = nullptr;
    QLabel *meshCount_ = nullptr;
    QPushButton *importMeshButton_ = nullptr;
    QPushButton *deleteMeshButton_ = nullptr;
    QRadioButton *solidModelRadio_ = nullptr;
    QRadioButton *planarModelRadio_ = nullptr;
    QAction *surfaceAction_ = nullptr;
    bool displayModeChosen_ = false;
    QLabel *viewerStatus_ = nullptr;
    // The orientation control is implemented in the page source file.
    QWidget *axisIndicator_ = nullptr;
    QSplitter *mainSplitter_ = nullptr;
    QSplitter *settingsSplitter_ = nullptr;
    QTabWidget *workspaceTabs_ = nullptr;
    QMainWindow *viewportWindow_ = nullptr;
    QToolBar *viewToolbar_ = nullptr;
    QTabWidget *historyTabs_ = nullptr;
    QListWidget *historyList_ = nullptr;
    QWidget *historyHeader_ = nullptr;
    QPushButton *historyCollapseButton_ = nullptr;
    int expandedHistoryHeight_ = 145;
    bool historyCollapsed_ = false;
};

} // namespace aerosim
