#include "aerosim/visualization/data/BoundaryLoadService.h"
#include "aerosim/visualization/data/BoundaryMeshReader.h"
#include <QThread>
#include <QUuid>
#include <exception>
namespace aerosim { namespace visualization {
struct BoundaryLoadService::Job {
    QString id,path,error;
    std::atomic_bool cancelled{false};
    std::shared_ptr<const BoundaryMesh> result;
};
BoundaryLoadService::BoundaryLoadService(QObject* parent):QObject(parent) {}
BoundaryLoadService::~BoundaryLoadService() {
    if(job_) job_->cancelled=true;
    if(thread_) { thread_->disconnect(this); thread_->wait(); delete thread_; }
}
bool BoundaryLoadService::busy() const { return thread_!=nullptr; }
std::shared_ptr<const BoundaryMesh> BoundaryLoadService::mesh(const QString& id) const { return meshes_.value(id); }
void BoundaryLoadService::load(const QString& path) {
    Q_ASSERT(QThread::currentThread()==thread());
    if(busy()) { emit failed(QString(),QString::fromUtf8("请等待当前导入完成，或移除正在加载的对象以取消")); return; }
    auto job=std::make_shared<Job>(); job_=job;
    job->id=QUuid::createUuid().toString(QUuid::WithoutBraces); job->path=path;
    thread_=QThread::create([job] {
        try { job->result=BoundaryMeshReader::read(job->path,&job->cancelled); }
        catch(const std::exception& e) { job->error=QString::fromUtf8(e.what()); }
        catch(...) { job->error=QString::fromUtf8("未知网格读取错误"); }
    });
    auto* worker=thread_;
    worker->setParent(this); // Also owns completed threads awaiting deleteLater().
    // Only a completion notification crosses threads. The private result is
    // consumed after wait(), never mutated concurrently with the GUI.
    connect(worker,&QThread::finished,this,[this,job,worker] {
        worker->wait(); thread_=nullptr; job_.reset(); worker->deleteLater();
        if(!job->cancelled && job->result) {
            meshes_.insert(job->id,job->result);
            emit loaded(job->id,job->path,job->result->summary());
        } else emit failed(job->id,job->cancelled?QString::fromUtf8("已取消导入"):job->error);
        emit busyChanged(false);
    },Qt::QueuedConnection);
    emit busyChanged(true); emit started(job->id,path); worker->start();
}
void BoundaryLoadService::remove(const QString& id) {
    Q_ASSERT(QThread::currentThread()==thread());
    if(job_ && job_->id==id) job_->cancelled=true;
    meshes_.remove(id);
}
} }
