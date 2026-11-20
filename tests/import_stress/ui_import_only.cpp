#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include "MeshViewerBinding.h"
#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/MeshManagementPage.h"
#include "aerosim/visualization/data/BoundaryLoadService.h"
#include "aerosim/visualization/scene/BoundaryPolyDataMapper.h"
#include "aerosim/visualization/scene/VtkSceneViewer.h"

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkIdTypeArray.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindow.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace aerosim;
using namespace aerosim::visualization;

static void check(bool ok, const QString& msg) {
    if (!ok) throw std::runtime_error(msg.toUtf8().constData());
}

template<class T> static T* child(QObject& parent, const char* name = nullptr) {
    auto* p = name ? parent.findChild<T*>(QString::fromLatin1(name)) : parent.findChild<T*>();
    check(p != nullptr, QStringLiteral("Missing UI component: ") + (name ? name : T::staticMetaObject.className()));
    return p;
}

class DialogFilter : public QObject {
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::Polish) {
            if (auto* dlg = qobject_cast<QFileDialog*>(obj)) {
                dlg->setAttribute(Qt::WA_ShowWithoutActivating);
                dlg->setAttribute(Qt::WA_DontShowOnScreen);
            }
        }
        return false;
    }
};

static void phase(const char* name) {
    std::fprintf(stderr, "mesh import UI: %s\n", name);
    std::fflush(stderr);
}

static qint64 differentPixels(const QImage& before, const QImage& after) {
    check(!before.isNull() && !after.isNull(), "Missing GL framebuffer");
    const QImage blank = before.scaled(after.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    const QImage frame = after.convertToFormat(QImage::Format_RGB32);
    qint64 changed = 0;
    for (int y = frame.height() / 5; y < frame.height() * 4 / 5; ++y) {
        const QRgb* ref = reinterpret_cast<const QRgb*>(blank.constScanLine(y));
        const QRgb* out = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
        for (int x = frame.width() / 5; x < frame.width() * 4 / 5; ++x) {
            const int delta = std::max(std::abs(qRed(ref[x]) - qRed(out[x])),
                              std::max(std::abs(qGreen(ref[x]) - qGreen(out[x])), std::abs(qBlue(ref[x]) - qBlue(out[x]))));
            if (delta > 12) ++changed;
        }
    }
    return changed;
}

static void mappings(vtkDataSetAttributes* a, const char* name, vtkIdType count) {
    auto* ids = vtkIdTypeArray::SafeDownCast(a->GetArray(name));
    check(ids && ids->GetNumberOfTuples() == count, "Missing full provenance array");
    for (auto i : {vtkIdType(0), count / 2, count - 1})
        check(ids->GetValue(i) == i, "Surface provenance mismatch");
}

static std::array<double, 11> cameraPose(vtkCamera* camera) {
    std::array<double, 11> coordinates;
    camera->GetPosition(coordinates.data());
    camera->GetFocalPoint(coordinates.data() + 3);
    camera->GetViewUp(coordinates.data() + 6);
    coordinates[9] = camera->GetParallelScale();
    coordinates[10] = camera->GetViewAngle();
    return coordinates;
}

static void recordFrame(const char* modeName, const QImage& frame, QJsonObject metrics) {
    metrics["record"] = "frame";
    metrics["displayMode"] = modeName;
    PROCESS_MEMORY_COUNTERS_EX counters = {};
    counters.cb = sizeof(counters);
    check(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                               sizeof(counters)) != 0, "Could not measure process memory");
    metrics["privateCommitBytes"] = double(counters.PrivateUsage);
    metrics["peakPrivateCommitBytes"] = double(counters.PeakPagefileUsage);
    metrics["workingSetBytes"] = double(counters.WorkingSetSize);
    metrics["peakWorkingSetBytes"] = double(counters.PeakWorkingSetSize);
    if (qEnvironmentVariableIsSet("AEROSIM_MESH_ACCEPTANCE_CAPTURE")) {
        QBuffer buffer;
        check(buffer.open(QIODevice::WriteOnly) && frame.save(&buffer, "PNG"), "Could not encode framebuffer");
        metrics["framePngBase64"] = QString::fromLatin1(buffer.data().toBase64());
    }
    std::puts(QJsonDocument(metrics).toJson(QJsonDocument::Compact).constData());
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    configureMeshViewerSurface();
    QApplication app(argc, argv);
    DialogFilter filter;
    app.installEventFilter(&filter);
    auto args = app.arguments();
    const bool show = args.removeAll("--show") != 0;
    QJsonObject result{{"mode", "Qt-file-dialog-import-only"}};
    QElapsedTimer total; total.start();
    bool passed = false;
    try {
        check(args.size() == 5, "Usage: brep_gui_probe [--show] <vtp> <points> <triangles> <expected-undirected-edges>");
        const QFileInfo file(args[1]);
        const QString path = file.canonicalFilePath();
        const qint64 points = args[2].toLongLong(), faces = args[3].toLongLong();
        const qint64 expectedEdges = args[4].toLongLong();
        check(file.isFile() && points > 0 && faces > 0 && expectedEdges > 0, "Invalid input arguments");
        result["path"] = path;
        result["fileBytes"] = double(file.size());
        QString settings = qEnvironmentVariable("AEROSIM_PROBE_SETTINGS");
        if (settings.isEmpty()) {
            settings = QDir::temp().filePath(QStringLiteral("aerosim-import-probe-%1")
                                                 .arg(QCoreApplication::applicationPid()));
        }
        check(QDir(settings).exists() || QDir().mkpath(settings),
              QStringLiteral("Cannot create isolated probe settings directory: ") + settings);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings);
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings);
        MainWindow window;
        check(installMeshViewer(window), "Cannot install production viewer");
        auto* page = child<MeshManagementPage>(window);
        auto* viewer = child<VtkSceneViewer>(window);
        auto* loader = child<BoundaryLoadService>(*viewer);
        auto* viewport = child<QVTKOpenGLNativeWidget>(*viewer);
        auto* button = child<QPushButton>(*page, "importMeshButton");
        auto* table = child<QTableWidget>(*page, "meshTable");
        auto* renderWindow = viewport->GetRenderWindow();
        auto* renderer = renderWindow->GetRenderers()->GetFirstRenderer();
        check(renderer && table->rowCount() == 0, "Unexpected initial scene");
        QImage blank;
        {
            window.setAttribute(Qt::WA_ShowWithoutActivating);
            window.setAttribute(Qt::WA_DontShowOnScreen, !show);
            window.resize(1000, 720);
            window.show();
            app.processEvents();
            blank = viewport->grabFramebuffer();
            check(viewport->isValid() && !blank.isNull(), "Missing functional OpenGL context");
            viewport->makeCurrent();
            const auto* gl = viewport->context()->functions()->glGetString(GL_RENDERER);
            check(gl != nullptr, "Could not identify GL renderer");
            result["openGLRenderer"] = QString::fromLatin1(reinterpret_cast<const char*>(gl));
            viewport->doneCurrent();
        }
        QString selected, loadedId, failure;
        QElapsedTimer importing;
        QObject::connect(page, &MeshManagementPage::boundaryImportRequested, &app,
            [&](const QStringList& paths) { if (paths.size() == 1) selected = QFileInfo(paths.first()).canonicalFilePath(); });
        QObject::connect(loader, &BoundaryLoadService::started, &app,
            [&](const QString&, const QString&) { importing.start(); phase("service started"); });
        QObject::connect(loader, &BoundaryLoadService::loaded, &app,
            [&](const QString& id, const QString&, const QString&) { loadedId = id; phase("row and actor created"); });
        QObject::connect(loader, &BoundaryLoadService::failed, &app,
            [&](const QString&, const QString& reason) { failure = reason; });

        QTimer rejectChooser;
        rejectChooser.setInterval(50);
        QObject::connect(&rejectChooser, &QTimer::timeout, [&] {
            if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {rejectChooser.stop();dialog->reject();}
        });
        rejectChooser.start();button->click();rejectChooser.stop();
        check(!loader->busy() && table->rowCount()==0 && button->isEnabled() && !importing.isValid(), "Cancelled chooser started import");
        result["chooserCancelChecked"] = true;
        bool selectedDialog = false;
        QTimer choose, cancelDialog;
        choose.setInterval(70);
        QObject::connect(&choose, &QTimer::timeout, &app, [&] {
            auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            dialog->setDirectory(file.absolutePath());
            dialog->selectFile(file.fileName());
            selectedDialog = true;
            choose.stop();
            QTimer::singleShot(120, dialog, [dialog] { QMetaObject::invokeMethod(dialog, "accept"); });
        });
        cancelDialog.setSingleShot(true);
        QObject::connect(&cancelDialog, &QTimer::timeout, &app, [&] {
            if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) dialog->reject();
        });
        choose.start(); cancelDialog.start(10000);
        button->click(); // Runs the application's real QFileDialog modal event loop.
        choose.stop(); cancelDialog.stop();
        check(selectedDialog && selected == path && importing.isValid(), "File dialog did not start the expected import");
        while (loader->busy() && importing.elapsed() < 590000) {
            app.processEvents(QEventLoop::AllEvents, 40);
            QThread::msleep(2);
        }
        check(!loader->busy(), "UI import timed out");
        check(failure.isEmpty(), failure);
        auto mesh = loader->mesh(loadedId);
        check(mesh && mesh->surface && !mesh->volume, "Missing surface in production service");
        result["importAndUiMs"] = double(importing.elapsed());
        auto* row = table->item(0, 1);
        check(table->rowCount() == 1 && row && row->text() == file.fileName() &&
              row->data(Qt::UserRole).toString() == loadedId, "Ready table row mismatch");
        auto* actors = renderer->GetActors();
        actors->InitTraversal();
        auto* actor = actors->GetNextActor();
        auto* mapper = actor ? BoundaryPolyDataMapper::SafeDownCast(actor->GetMapper()) : nullptr;
        check(actors->GetNumberOfItems() == 1 && actor->GetVisibility() && mapper &&
              mapper->GetInput() == mesh->surface.GetPointer(), "Full mesh not attached to shared-edge mapper");
        result["initialEdgeOverlay"] = bool(actor->GetProperty()->GetEdgeVisibility());
        result["initialSurfaceRepresentation"] = actor->GetProperty()->GetRepresentation() == VTK_SURFACE;
        auto* s = mesh->surface.GetPointer();
        check(mesh->inputPoints == points && mesh->inputCells == faces && mesh->uniqueBoundaryPoints == points &&
              mesh->boundaryFaces == faces && s->GetNumberOfPolys() == faces && s->GetNumberOfPoints() == points,
              "Point and face count mismatch");
        mappings(s->GetPointData(), "vtkOriginalPointIds", points);
        mappings(s->GetCellData(), "vtkOriginalCellIds", faces);
        auto* pn = s->GetPointData()->GetNormals();
        auto* fn = s->GetCellData()->GetNormals();
        auto* area = s->GetCellData()->GetArray("FaceArea");
        check(pn && fn && area && pn->GetNumberOfTuples() == points && fn->GetNumberOfTuples() == faces &&
              area->GetNumberOfTuples() == faces && area->GetTuple1(faces - 1) > 0.0, "Mesh fields missing");
        result["points"] = double(points);
        result["triangles"] = double(faces);
        result["readyRows"] = table->rowCount();
        result["actors"] = actors->GetNumberOfItems();
        result["fieldsAndProvenanceChecked"] = true;
        check(button->isEnabled(), "Import button did not recover after success");
        failure.clear();
        loader->load(path + ".missing");
        while(loader->busy()) { app.processEvents(); QThread::msleep(2); }
        check(!failure.isEmpty() && table->rowCount()==1 && button->isEnabled(), "Failed import left a row or disabled the button");
        loader->load(path);
        while(loader->busy()) { app.processEvents(); QThread::msleep(2); }
        check(table->rowCount()==2 && button->isEnabled(), "UI failed to recover after error");
        result["uiErrorRecoveryChecked"] = true;
        passed = true;
    } catch (const std::exception& e) { result["error"] = QString::fromUtf8(e.what()); }
    result["passed"] = passed;
    result["elapsedMs"] = double(total.elapsed());
    std::puts(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    return passed ? 0 : 1;
}
