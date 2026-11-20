#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include "aerosim/visualization/data/BoundaryLoadService.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QEvent>
#include <QTimer>
#include <cstdio>
#include <stdexcept>
using namespace aerosim::visualization;
static void record(QJsonObject r){std::puts(QJsonDocument(r).toJson(QJsonDocument::Compact).constData());std::fflush(stdout);}
static double memory(){PROCESS_MEMORY_COUNTERS_EX m{};m.cb=sizeof(m);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m));return double(m.PrivateUsage);}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv); SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try{
  if(argc!=2)throw std::runtime_error("Expected manifest JSON");
  QFile file(app.arguments()[1]);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read manifest");
  auto spec=QJsonDocument::fromJson(file.readAll()).object();auto cases=spec["cases"].toArray();
  auto service=new BoundaryLoadService;QStringList paths,ids;int next=0,completed=0;bool passed=true;QElapsedTimer all,one;all.start();
  HANDLE locked=INVALID_HANDLE_VALUE;
  if(spec.contains("lock")){auto p=spec["lock"].toString();locked=CreateFileW(reinterpret_cast<LPCWSTR>(p.utf16()),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);if(locked==INVALID_HANDLE_VALUE)throw std::runtime_error("lock setup failed");}
  for(auto value:cases)paths<<value.toObject()["path"].toString();
  QObject::connect(service,&BoundaryLoadService::started,[&](const QString&activeId,const QString&){one.restart();
   if(spec.contains("cancelMs"))QTimer::singleShot(spec["cancelMs"].toInt(),service,[service,activeId]{service->remove(activeId);});
   if(next++==0 && spec.contains("mutate")){auto m=spec["mutate"].toObject();auto p=m["path"].toString();auto op=m["op"].toString();bool ok=false;
    if(op=="delete")ok=QFile::remove(p);else if(op=="move")ok=QFile::rename(p,p+".moved");else{QFile f(p);ok=f.open(QIODevice::WriteOnly|QIODevice::Truncate);if(ok)f.write("truncated");}
    if(!ok){passed=false;record({{"setupError","queued mutation failed"}});}
   }
  });
  QObject::connect(service,&BoundaryLoadService::loaded,[&](const QString&id,const QString&path,const QString&){
   auto c=cases[completed++].toObject();auto mesh=service->mesh(id);bool ok=c["expect"].toString()!="reject" && bool(mesh);
   if(mesh){if(c.contains("points"))ok &= mesh->inputPoints==qint64(c["points"].toDouble());if(c.contains("cells"))ok &= mesh->inputCells==qint64(c["cells"].toDouble());if(c.contains("faces"))ok &= mesh->boundaryFaces==qint64(c["faces"].toDouble());}
   passed &=ok;ids<<id;record({{"record","case"},{"name",c["name"]},{"path",path},{"accepted",true},{"passed",ok},{"elapsedMs",double(one.elapsed())},{"points",mesh?double(mesh->inputPoints):0},{"cells",mesh?double(mesh->inputCells):0},{"faces",mesh?double(mesh->boundaryFaces):0},{"privateBytes",memory()}});
  });
  QObject::connect(service,&BoundaryLoadService::failed,[&](const QString&id,const QString&error){auto c=cases[completed++].toObject();bool ok=c["expect"].toString()!="accept" && !service->mesh(id) && !error.isEmpty();passed &=ok;record({{"record","case"},{"name",c["name"]},{"accepted",false},{"passed",ok},{"error",error},{"elapsedMs",double(one.elapsed())},{"privateBytes",memory()}});});
  service->loadFiles(paths);
  if(spec["destroyActive"].toBool()){QThread::msleep(spec["destroyDelayMs"].toInt());delete service;record({{"record","summary"},{"passed",true},{"destroyActiveMs",double(all.elapsed())}});return 0;}
  while(service->busy() && all.elapsed()<600000){app.processEvents();QThread::msleep(1);}
  passed &= !service->busy() && completed==cases.size();
  double retained=memory();for(const auto&id:ids){service->remove(id);passed &= !service->mesh(id);}app.sendPostedEvents(nullptr,QEvent::DeferredDelete);
  record({{"record","summary"},{"passed",passed},{"completed",completed},{"elapsedMs",double(all.elapsed())},{"retainedPrivateBytes",retained},{"releasedPrivateBytes",memory()}});
  delete service;if(locked!=INVALID_HANDLE_VALUE)CloseHandle(locked);return passed?0:1;
 }catch(const std::exception&e){record({{"passed",false},{"error",e.what()}});return 1;}
}
