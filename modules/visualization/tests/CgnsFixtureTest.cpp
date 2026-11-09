#include "aerosim/visualization/DataAdapter.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QDebug>
#include <cgnslib.h>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <cmath>
#define CHECK(x) do { if (!(x)) { qCritical() << "check failed:" << #x << __LINE__; return 1; } } while(false)
#define CG(x) do { if ((x)!=CG_OK) { qCritical() << cg_get_error() << __LINE__; return 1; } } while(false)
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir dir; CHECK(dir.isValid());
    const QString path=dir.filePath(QStringLiteral("sample.cgns"));
    int file=0,base=0,zone=0,section=0,solution=0,field=0,coord=0;
    CG(cg_open(QFile::encodeName(path).constData(),CG_MODE_WRITE,&file));
    CG(cg_base_write(file,"Base",3,3,&base));
    cgsize_t sizes[3]={4,1,0}; CG(cg_zone_write(file,base,"Zone",sizes,CGNS_ENUMV(Unstructured),&zone));
    const double x[4]={0,1,0,0},y[4]={0,0,1,0},z[4]={0,0,0,1},p[4]={1,2,3,4};
    CG(cg_coord_write(file,base,zone,CGNS_ENUMV(RealDouble),"CoordinateX",x,&coord));
    CG(cg_coord_write(file,base,zone,CGNS_ENUMV(RealDouble),"CoordinateY",y,&coord));
    CG(cg_coord_write(file,base,zone,CGNS_ENUMV(RealDouble),"CoordinateZ",z,&coord));
    const cgsize_t tetra[4]={1,2,3,4},triangle[3]={1,2,3};
    CG(cg_section_write(file,base,zone,"volume",CGNS_ENUMV(TETRA_4),1,1,0,tetra,&section));
    CG(cg_section_write(file,base,zone,"wall",CGNS_ENUMV(TRI_3),2,2,0,triangle,&section));
    CG(cg_sol_write(file,base,zone,"Flow",CGNS_ENUMV(Vertex),&solution));
    CG(cg_field_write(file,base,zone,solution,CGNS_ENUMV(RealDouble),"Pressure",p,&field));
    CG(cg_close(file));
    auto adapter=aerosim::visualization::createCgnsAdapter();
    CHECK(adapter->accepts(path)); CHECK(!adapter->accepts(QStringLiteral("mesh.vtu")));
    CHECK(!adapter->read(dir.filePath(QStringLiteral("missing.cgns"))).ok());
    const auto result=adapter->read(path); if(!result.ok()) qCritical()<<result.error;
    CHECK(result.ok() && result.dataset.regions.size()==2);
    const auto &volume=result.dataset.regions[0]; const auto &wall=result.dataset.regions[1];
    CHECK(volume.dimension==3 && volume.data->GetNumberOfPoints()==4 && volume.data->GetNumberOfCells()==1);
    CHECK(wall.dimension==2 && wall.data->GetNumberOfPoints()==3 && wall.data->GetNumberOfCells()==1);
    CHECK(volume.fields.size()==1 && wall.fields.size()==1);
    CHECK(volume.fields[0].association==QStringLiteral("point") && wall.fields[0].hasRange);
    CHECK(std::abs(wall.fields[0].minimum-1)<1e-9 && std::abs(wall.fields[0].maximum-3)<1e-9);
    CHECK(wall.data->GetPointData()->GetArray("Pressure")->GetNumberOfTuples()==3);
    return 0;
}
