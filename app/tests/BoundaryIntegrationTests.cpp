#include "BoundaryViewerBinding.h"
#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/MeshManagementPage.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include "aerosim/visualization/scene/VtkSceneViewer.h"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>
#include <QToolButton>
#include <QVTKOpenGLNativeWidget.h>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace aerosim;
using namespace aerosim::visualization;
static void require(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
static void waitFor(const std::function<bool()>& condition) {
    QElapsedTimer timer;timer.start();
    while(!condition() && timer.elapsed()<15000) {QApplication::processEvents();QThread::msleep(5);}
    require(condition(),"timed out waiting for GUI result");
}
static void selectFile(QWidget* owner,const QString& path) {
    QTimer::singleShot(150,owner,[owner,path]{
        auto* dialog=owner->findChild<QFileDialog*>();
        if(dialog) {
            // QFileSystemModel populates directories asynchronously. Enter the
            // absolute filename as a user would, then use the Open button.
            auto* name=dialog->findChild<QLineEdit*>("fileNameEdit");
            if(name) name->setText(QDir::toNativeSeparators(path));
            QTimer::singleShot(100,dialog,[dialog]{
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Open)->click();
            });
            QTimer::singleShot(5000,dialog,&QDialog::reject);
        }
    });
}
int main(int argc,char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc,argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    try {
        const QString fixture=QDir(QString::fromUtf8(AEROSIM_FIXTURE_DIR)).absoluteFilePath("cube_t00.vtk");
        MainWindow main;require(installBoundaryViewer(main),"main viewport install");main.resize(1440,960);main.show();
        auto* page=main.findChild<MeshManagementPage*>();auto* view=main.findChild<VtkSceneViewer*>();
        require(page&&view,"main view missing");
        auto* add=page->findChild<QPushButton*>("addMeshButton");
        selectFile(page,fixture);add->click();waitFor([&]{return view->objectCount()==1;});
        auto* tree=page->findChild<QTreeWidget*>("meshList");require(tree->topLevelItemCount()==1,"mock items retained");
        const QString first=tree->topLevelItem(0)->data(0,Qt::UserRole).toString();
        require(view->displayedFaces(first)==24,"main wrong surface");
        require(view->objectStyle(first).value("visible").toBool(),"imported mesh hidden by initial tree signals");
        selectFile(page,fixture);add->click();waitFor([&]{return view->objectCount()==2;});
        const QString second=tree->topLevelItem(1)->data(0,Qt::UserRole).toString();require(first!=second,"unstable IDs");
        page->findChild<QSlider*>("meshOpacityEditor")->setValue(25);
        require(std::abs(view->objectStyle(second).value("opacity").toDouble()-25)<0.01,"opacity not applied");
        require(view->objectStyle(first).value("opacity").toDouble()==100,"other object changed");
        page->findChild<QCheckBox*>("meshVisibleEditor")->setChecked(false);
        require(!view->objectStyle(second).value("visible").toBool(),"visibility not applied");
        page->findChild<QPushButton*>("removeMeshButton")->click();
        require(view->objectCount()==1 && tree->topLevelItemCount()==1,"remove not synchronized");
        require(view->objectStyle(first).value("visible").toBool(),"removing another mesh hid the remaining mesh");
        const auto captures=qEnvironmentVariable("AEROSIM_BOUNDARY_CAPTURE_DIR");
        if(!captures.isEmpty()) {
            require(QDir().mkpath(captures),"capture directory");QApplication::processEvents();
            require(main.grab().save(captures+"/boundary-main.png"),"main screenshot save");
        }
        main.hide();
        PostProcessWorkbenchWindow post;require(installBoundaryViewer(post),"post viewport install");post.resize(1440,960);post.show();
        auto* pv=post.findChild<VtkSceneViewer*>();
        require(!post.findChild<QToolButton*>("postProcessPlayFrame")->isEnabled(),"mock timeline enabled in real viewer");
        selectFile(&post,fixture);post.findChild<QAction*>("postProcessImportAction")->trigger();waitFor([&]{return pv->objectCount()==1;});
        auto* list=post.findChild<QListWidget*>("sceneLayerList");require(list->count()==1,"post mock objects retained");
        const auto id=list->item(0)->data(Qt::UserRole+1).toString();require(pv->displayedFaces(id)==24,"post wrong surface");
        post.findChild<QAction*>("postProcessMeshAction")->trigger();require(pv->objectStyle(id).value("mode").toInt()==0,"edges not toggled");
        post.findChild<QAction*>("postProcessObjectStyleAction")->trigger();
        auto* dialog=post.findChild<QDialog*>("postProcessObjectStyleDialog");require(dialog,"style dialog absent");
        dialog->findChild<QSpinBox*>("objectStyleOpacity")->setValue(50);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        require(pv->objectStyle(id).value("opacity").toDouble()==50,"post style not applied");
        require(post.findChild<QTreeWidget*>("postProcessDatasetTree")->topLevelItemCount()==1,"data tree not synchronized");
        post.findChild<QAction*>("postProcessMeshAction")->trigger();
        require(pv->objectStyle(id).value("mode").toInt()==2,"edges not restored");
        if(!captures.isEmpty()) {
            QApplication::processEvents();
            require(post.grab().save(captures+"/boundary-postprocess.png"),"post screenshot save");
        }
        std::cout<<"Boundary GUI integration passed: dialogs, both windows, real surfaces, independent styles, removal\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
