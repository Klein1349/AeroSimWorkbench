#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include "aerosim/visualization/data/BoundaryLoadService.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <cstdio>
#include <stdexcept>
using namespace aerosim::visualization;
static void require(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
static double memory() {
    PROCESS_MEMORY_COUNTERS_EX m{}; m.cb=sizeof(m);
    require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m)),"memory measurement failed");
    return double(m.PrivateUsage);
}
static void emitRecord(QJsonObject r) { std::puts(QJsonDocument(r).toJson(QJsonDocument::Compact).constData()); std::fflush(stdout); }
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        require(argc==5,"Usage: lifecycle_probe <mesh> <points> <faces> <cycles>");
        auto args=app.arguments(); int cycles=args[4].toInt(); require(cycles>0,"invalid cycles");
        BoundaryLoadService service;
        QString id,error; int loaded=0,failed=0;
        QObject::connect(&service,&BoundaryLoadService::loaded,[&](const QString& value,const QString&,const QString&){id=value;++loaded;});
        QObject::connect(&service,&BoundaryLoadService::failed,[&](const QString&,const QString& value){error=value;++failed;});
        auto wait=[&]{QElapsedTimer t;t.start();while(service.busy() && t.elapsed()<300000){app.processEvents();QThread::msleep(1);}require(!service.busy(),"import timed out");app.sendPostedEvents(nullptr,QEvent::DeferredDelete);};
        double firstReleased=0,lastReleased=0;
        for(int i=0;i<cycles;++i) {
            id.clear(); error.clear(); QElapsedTimer timer;timer.start();service.load(args[1]);wait();
            require(error.isEmpty() && !id.isEmpty(),"cycle load failed");
            auto mesh=service.mesh(id);
            require(mesh && mesh->inputPoints==args[2].toLongLong() && mesh->inputCells==args[3].toLongLong(),"mesh counts mismatch");
            std::weak_ptr<const BoundaryMesh> weak=mesh;
            double loadedBytes=memory();mesh.reset();service.remove(id);
            require(!service.mesh(id) && weak.expired(),"removed mesh retained");
            lastReleased=memory();if(i==0)firstReleased=lastReleased;
            emitRecord({{"record","cycle"},{"cycle",i+1},{"elapsedMs",double(timer.elapsed())},{"loadedPrivateBytes",loadedBytes},{"releasedPrivateBytes",lastReleased}});
        }
        // A failed request must not block the following valid request in the same queue.
        int beforeLoaded=loaded,beforeFailed=failed;id.clear();
        service.loadFiles({args[1]+".missing",args[1]});wait();
        require(loaded==beforeLoaded+1 && failed==beforeFailed+1 && service.mesh(id),"queue failed to recover after invalid file");
        service.remove(id);
        // Cancel an active request using its real service ID; then load again.
        QString cancelled;
        auto conn=QObject::connect(&service,&BoundaryLoadService::started,[&](const QString& value,const QString&){cancelled=value;service.remove(value);});
        beforeFailed=failed;service.load(args[1]);wait();QObject::disconnect(conn);
        require(failed==beforeFailed+1 && !service.mesh(cancelled),"cancel retained a mesh");
        beforeLoaded=loaded;service.load(args[1]);wait();require(loaded==beforeLoaded+1 && service.mesh(id),"post-cancel recovery failed");service.remove(id);
        emitRecord({{"record","summary"},{"passed",true},{"cycles",cycles},{"releasedGrowthBytes",lastReleased-firstReleased},{"invalidQueueRecovery",true},{"cancelRecovery",true},{"ownershipReleased",true}});
        return 0;
    }catch(const std::exception& e){emitRecord({{"passed",false},{"error",e.what()}});return 1;}
}
