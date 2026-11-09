#if defined(_MSC_VER) && !defined(_DEBUG)
#error The current CGNS/VTK integration is verified for Windows Debug only.
#endif
#include "aerosim/visualization/DataAdapter.h"
#include <QFile>
#include <cgnslib.h>
#include <vtkCellType.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>
#include <vtkStructuredGrid.h>
#include <array>
#include <cmath>
#include <vector>

namespace aerosim { namespace visualization {
namespace {
struct CgnsFile { int id = 0; ~CgnsFile() { if (id) cg_close(id); } };
QString reason(const QString &prefix) { return prefix + QStringLiteral(": ") + QString::fromLocal8Bit(cg_get_error()); }
int vtkType(CGNS_ENUMT(ElementType_t) type) {
    switch (type) {
    case CGNS_ENUMV(NODE): return VTK_VERTEX;
    case CGNS_ENUMV(BAR_2): return VTK_LINE;
    case CGNS_ENUMV(TRI_3): return VTK_TRIANGLE;
    case CGNS_ENUMV(QUAD_4): return VTK_QUAD;
    case CGNS_ENUMV(TETRA_4): return VTK_TETRA;
    case CGNS_ENUMV(PYRA_5): return VTK_PYRAMID;
    case CGNS_ENUMV(PENTA_6): return VTK_WEDGE;
    case CGNS_ENUMV(HEXA_8): return VTK_HEXAHEDRON;
    default: return -1;
    }
}
int dimension(CGNS_ENUMT(ElementType_t) type) {
    if (type == CGNS_ENUMV(TRI_3) || type == CGNS_ENUMV(QUAD_4)) return 2;
    if (type == CGNS_ENUMV(BAR_2)) return 1;
    if (type == CGNS_ENUMV(NODE)) return 0;
    return 3;
}
class CgnsAdapter final : public DataAdapter {
public:
    bool accepts(const QString &path) const override { return path.endsWith(QStringLiteral(".cgns"), Qt::CaseInsensitive); }
    ImportResult read(const QString &path) const override {
        ImportResult result; result.dataset.path = path;
        CgnsFile file;
        if (cg_open(QFile::encodeName(path).constData(), CG_MODE_READ, &file.id) != CG_OK) { file.id = 0; result.error = reason(QStringLiteral("CGNS open failed")); return result; }
        int bases = 0; if (cg_nbases(file.id, &bases) != CG_OK) { result.error = reason(QStringLiteral("CGNS bases")); return result; }
        for (int b = 1; b <= bases; ++b) {
            char baseName[33] = {}; int cellDim = 0, physicalDim = 0;
            if (cg_base_read(file.id, b, baseName, &cellDim, &physicalDim) != CG_OK) { result.error = reason(QStringLiteral("CGNS base")); return result; }
            int zones = 0; if (cg_nzones(file.id, b, &zones) != CG_OK) { result.error = reason(QStringLiteral("CGNS zones")); return result; }
            for (int z = 1; z <= zones; ++z) {
                char zoneName[33] = {}; cgsize_t sizes[9] = {};
                CGNS_ENUMT(ZoneType_t) zoneType;
                if (cg_zone_read(file.id,b,z,zoneName,sizes) != CG_OK || cg_zone_type(file.id,b,z,&zoneType) != CG_OK) { result.error = reason(QStringLiteral("CGNS zone")); return result; }
                if (cellDim != 3 || physicalDim != 3) { result.error = QStringLiteral("Only 3D CGNS datasets are currently supported"); return result; }
                if (zoneType != CGNS_ENUMV(Unstructured)) { result.error = QStringLiteral("Unsupported CGNS zone type (only unstructured zones are currently supported): ") + QString::fromLocal8Bit(zoneName); return result; }
                const cgsize_t count = sizes[0];
                if (count <= 0 || sizes[1] < 0) { result.error = QStringLiteral("Invalid CGNS zone size"); return result; }
                cgsize_t lo[3] = {1,1,1}, hi[3] = {count,1,1};
                std::array<std::vector<double>,3> xyz;
                const char *coordinates[] = {"CoordinateX","CoordinateY","CoordinateZ"};
                for (int i=0;i<3;++i) { xyz[i].assign(static_cast<size_t>(count),0.0); if (i < physicalDim && cg_coord_read(file.id,b,z,coordinates[i],CGNS_ENUMV(RealDouble),lo,hi,xyz[i].data()) != CG_OK) { result.error=reason(QStringLiteral("CGNS coordinates")); return result; } }
                auto points = vtkSmartPointer<vtkPoints>::New(); points->SetDataTypeToDouble(); points->SetNumberOfPoints(static_cast<vtkIdType>(count));
                for (cgsize_t i=0;i<count;++i) points->SetPoint(static_cast<vtkIdType>(i), xyz[0][i], xyz[1][i], xyz[2][i]);
                auto volume = vtkSmartPointer<vtkUnstructuredGrid>::New(); volume->SetPoints(points);
                QVector<RegionData> surfaces;
                QVector<QVector<cgsize_t>> surfacePointIds;
                int sections=0; if (cg_nsections(file.id,b,z,&sections) != CG_OK) { result.error=reason(QStringLiteral("CGNS sections")); return result; }
                cgsize_t nextVolumeElement = 1;
                for(int n=1;n<=sections;++n) {
                    char name[33]={}; CGNS_ENUMT(ElementType_t) type; cgsize_t first=0,last=0; int boundary=0,parent=0;
                    if(cg_section_read(file.id,b,z,n,name,&type,&first,&last,&boundary,&parent)!=CG_OK) { result.error=reason(QStringLiteral("CGNS section")); return result; }
                    int cellType=vtkType(type), nodes=0; if(cellType<0 || cg_npe(type,&nodes)!=CG_OK || nodes<=0) { result.error=QStringLiteral("Unsupported CGNS element section: ")+QString::fromLocal8Bit(name); return result; }
                    cgsize_t dataSize=0; if(cg_ElementDataSize(file.id,b,z,n,&dataSize)!=CG_OK || dataSize != (last-first+1)*nodes) { result.error=reason(QStringLiteral("CGNS connectivity size")); return result; }
                    std::vector<cgsize_t> connectivity(static_cast<size_t>(dataSize));
                    if(cg_elements_read(file.id,b,z,n,connectivity.data(),nullptr)!=CG_OK) { result.error=reason(QStringLiteral("CGNS connectivity")); return result; }
                    const bool isBoundary=dimension(type)<cellDim;
                    if (!isBoundary) {
                        if (first != nextVolumeElement) { result.error=QStringLiteral("CGNS volume sections must be contiguous and in element order"); return result; }
                        nextVolumeElement=last+1;
                    }
                    auto grid = isBoundary ? vtkSmartPointer<vtkUnstructuredGrid>::New() : volume;
                    vtkSmartPointer<vtkPoints> boundaryPoints;
                    std::vector<vtkIdType> localIds;
                    QVector<cgsize_t> sourcePointIds;
                    if(isBoundary) {
                        boundaryPoints=vtkSmartPointer<vtkPoints>::New(); boundaryPoints->SetDataTypeToDouble();
                        grid->SetPoints(boundaryPoints); localIds.assign(static_cast<size_t>(count),-1);
                    }
                    std::vector<vtkIdType> ids(static_cast<size_t>(nodes));
                    for(cgsize_t i=0;i<=last-first;++i) {
                        for(int k=0;k<nodes;++k) { const cgsize_t id=connectivity[static_cast<size_t>(i*nodes+k)]; if(id<1 || id>count) { result.error=QStringLiteral("Invalid CGNS connectivity index"); return result; } if(isBoundary) {
                            auto &local=localIds[static_cast<size_t>(id-1)];
                            if(local<0) { local=boundaryPoints->InsertNextPoint(xyz[0][id-1],xyz[1][id-1],xyz[2][id-1]); sourcePointIds.append(id-1); }
                            ids[k]=local;
                        } else ids[k]=static_cast<vtkIdType>(id-1); }
                        grid->InsertNextCell(cellType,nodes,ids.data());
                    }
                    if(isBoundary) { RegionData region; region.name=QString::fromLocal8Bit(name); region.id=QStringLiteral("%1/%2/%3").arg(b).arg(z).arg(n); region.dimension=dimension(type); region.data=grid; surfaces.append(region); surfacePointIds.append(sourcePointIds); }
                }
                RegionData region; region.name=QString::fromLocal8Bit(zoneName); region.id=QStringLiteral("%1/%2").arg(b).arg(z); region.dimension=cellDim; region.data=volume;
                int solutions=0; if(cg_nsols(file.id,b,z,&solutions)!=CG_OK) { result.error=reason(QStringLiteral("CGNS solutions")); return result; }
                if(solutions>1) { result.error=QStringLiteral("Multiple CGNS solutions / time steps are not yet supported"); return result; }
                for(int sol=1;sol<=solutions;++sol) {
                    char solName[33]={}; CGNS_ENUMT(GridLocation_t) location;
                    if(cg_sol_info(file.id,b,z,sol,solName,&location)!=CG_OK) { result.error=reason(QStringLiteral("CGNS solution")); return result; }
                    if(location!=CGNS_ENUMV(Vertex) && location!=CGNS_ENUMV(CellCenter)) { result.error=QStringLiteral("Unsupported CGNS field location"); return result; }
                    const bool point=location==CGNS_ENUMV(Vertex); cgsize_t upper[3]={point?count:sizes[1],1,1}; int fields=0;
                    if(cg_nfields(file.id,b,z,sol,&fields)!=CG_OK) { result.error=reason(QStringLiteral("CGNS fields")); return result; }
                    if(!point && volume->GetNumberOfCells()!=static_cast<vtkIdType>(sizes[1])) { result.error=QStringLiteral("Cell fields do not match CGNS volume sections"); return result; }
                    for(int f=1;f<=fields;++f) {
                        char name[33]={}; CGNS_ENUMT(DataType_t) datatype;
                        if(cg_field_info(file.id,b,z,sol,f,&datatype,name)!=CG_OK) { result.error=reason(QStringLiteral("CGNS field metadata")); return result; }
                        auto values=vtkSmartPointer<vtkDoubleArray>::New(); values->SetName(name); values->SetNumberOfTuples(static_cast<vtkIdType>(upper[0]));
                        if(cg_field_read(file.id,b,z,sol,name,CGNS_ENUMV(RealDouble),lo,upper,values->GetPointer(0))!=CG_OK) { result.error=reason(QStringLiteral("CGNS field")); return result; }
                        FieldInfo info; info.name=QString::fromLocal8Bit(name); info.association=point?QStringLiteral("point"):QStringLiteral("cell");
                        const double *range=values->GetRange(); info.hasRange=std::isfinite(range[0]) && std::isfinite(range[1]) && range[0]<=range[1];
                        if(info.hasRange) { info.minimum=range[0]; info.maximum=range[1]; }
                        region.fields.append(info);
                        if(point) {
                            volume->GetPointData()->AddArray(values);
                            for(int si=0;si<surfaces.size();++si) {
                                auto boundaryValues=vtkSmartPointer<vtkDoubleArray>::New(); boundaryValues->SetName(name);
                                boundaryValues->SetNumberOfTuples(surfacePointIds[si].size());
                                for(int pi=0;pi<surfacePointIds[si].size();++pi) boundaryValues->SetValue(pi,values->GetValue(static_cast<vtkIdType>(surfacePointIds[si][pi])));
                                surfaces[si].data->GetPointData()->AddArray(boundaryValues);
                                FieldInfo boundaryInfo=info; const double *boundaryRange=boundaryValues->GetRange();
                                boundaryInfo.hasRange=std::isfinite(boundaryRange[0]) && std::isfinite(boundaryRange[1]) && boundaryRange[0]<=boundaryRange[1];
                                if(boundaryInfo.hasRange) { boundaryInfo.minimum=boundaryRange[0]; boundaryInfo.maximum=boundaryRange[1]; }
                                surfaces[si].fields.append(boundaryInfo);
                            }
                        }
                        else volume->GetCellData()->AddArray(values);
                    }
                }
                result.dataset.regions.append(region); for(const auto &surface:surfaces) result.dataset.regions.append(surface);
            }
        }
        if(result.dataset.regions.isEmpty()) result.error=QStringLiteral("CGNS contains no readable regions");
        return result;
    }
};
}
std::unique_ptr<DataAdapter> createCgnsAdapter() { return std::unique_ptr<DataAdapter>(new CgnsAdapter); }
} }
