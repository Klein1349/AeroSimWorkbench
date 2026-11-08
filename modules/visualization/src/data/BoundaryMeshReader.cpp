#include "aerosim/visualization/data/BoundaryMeshReader.h"
#include <QFile>
#include <QFileInfo>
#include <vtkCallbackCommand.h>
#include <vtkAlgorithm.h>
#include <vtkCommand.h>
#include <vtkDataSetReader.h>
#include <vtkNew.h>
#include <vtkXMLGenericDataObjectReader.h>
#include <stdexcept>
namespace aerosim { namespace visualization {
std::shared_ptr<const BoundaryMesh> BoundaryMeshReader::read(const QString& path,const std::atomic_bool* cancelled) {
    if(!QFileInfo(path).isFile()) throw std::runtime_error("网格文件不存在");
    auto suffix=QFileInfo(path).suffix().toLower();
    struct State {const std::atomic_bool* cancel;bool error=false;} state{cancelled};
    vtkNew<vtkCallbackCommand> observer;
    observer->SetClientData(&state);
    observer->SetCallback([](vtkObject* object,unsigned long event,void* client,void*) {
        auto* s=static_cast<State*>(client);
        if(event==vtkCommand::ErrorEvent) s->error=true;
        if(s->cancel && s->cancel->load()) vtkAlgorithm::SafeDownCast(object)->SetAbortExecute(1);
    });
    vtkSmartPointer<vtkDataSet> data;
    const auto bytes=QFile::encodeName(path);
    if(QFile::decodeName(bytes)!=path) throw std::runtime_error("VTK 8.2 无法表示此文件路径");
    if(suffix=="vtk") {
        vtkNew<vtkDataSetReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer);
        reader->AddObserver(vtkCommand::ProgressEvent,observer);
        reader->SetFileName(bytes.constData()); reader->ReadAllScalarsOn(); reader->ReadAllVectorsOn(); reader->ReadAllFieldsOn();
        reader->Update(); data=reader->GetOutput();
    } else if(suffix=="vtu" || suffix=="vtp" || suffix=="vti") {
        vtkNew<vtkXMLGenericDataObjectReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer);
        reader->AddObserver(vtkCommand::ProgressEvent,observer);
        reader->SetFileName(bytes.constData()); reader->Update(); data=vtkDataSet::SafeDownCast(reader->GetOutput());
    } else throw std::runtime_error("支持的格式为 .vtk、.vtu、.vtp、.vti");
    if(state.error || !data) throw std::runtime_error("VTK 读取失败：文件损坏、类型不支持或路径编码不兼容");
    return BoundarySurfaceExtractor::extract(data,cancelled);
}
} }
