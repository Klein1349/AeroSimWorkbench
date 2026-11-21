#include "aerosim/visualization/features/SharedPolygonEdges.h"
#include "aerosim/visualization/features/BoundarySurfaceExtractor.h"
#include <vtkIdTypeArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkUnsignedCharArray.h>
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

using aerosim::visualization::SharedPolygonEdges;
using Edge = std::pair<std::uint32_t, std::uint32_t>;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

vtkSmartPointer<vtkPolyData> surface(unsigned pointCount, const std::vector<std::vector<vtkIdType>>& faces) {
    vtkNew<vtkPoints> points;
    for (unsigned i = 0; i < pointCount; ++i) points->InsertNextPoint(0, 0, 0);
    vtkNew<vtkCellArray> polys;
    for (const auto& face : faces) polys->InsertNextCell(static_cast<vtkIdType>(face.size()), face.data());
    auto mesh = vtkSmartPointer<vtkPolyData>::New();
    mesh->SetPoints(points);
    mesh->SetPolys(polys);
    return mesh;
}

void checkEdges(vtkPolyData* mesh, const std::set<Edge>& expected) {
    auto topology = SharedPolygonEdges::build(mesh);
    require(topology && topology->matches(mesh), "Missing topology");
    require(topology->edgeCount() == expected.size(), "Edge count mismatch");
    std::set<Edge> actual;
    topology->forEachBatch(mesh, [&](const std::uint32_t* ids, std::size_t count) {
        require(count % 2 == 0, "Split edge in a batch");
        for (std::size_t i = 0; i < count; i += 2) {
            require(ids[i] < ids[i + 1], "Unordered or zero-length topological edge");
            require(actual.emplace(ids[i], ids[i + 1]).second, "Duplicate edge");
        }
    });
    require(actual == expected, "Incorrect polygon edges");
}
}

int main() {
    try {
        // Three faces share 0-1 with both possible windings. All point
        // coordinates coincide: topological identity must follow IDs.
        auto mesh = surface(8, {{0,1,2}, {1,0,3,4}, {0,1,5,6,7}});
        checkEdges(mesh, {{0,1},{1,2},{0,2},{0,3},{3,4},{1,4},{1,5},{5,6},{6,7},{0,7}});
        auto topology = SharedPolygonEdges::build(mesh);
        vtkNew<vtkPolyData> shallow;
        shallow->ShallowCopy(mesh);
        require(topology->matches(shallow), "Shallow-copy geometry should reuse topology");
        shallow->GetPolys()->GetData()->Modified();
        require(!topology->matches(mesh), "Changed connectivity should invalidate the mask");

        // A volume filter may produce separate render vertices on each patch.
        // Only a verified source mapping identifies topologically shared points.
        auto seam = surface(6, {{0,1,2}, {3,4,5}});
        require(SharedPolygonEdges::build(seam)->edgeCount() == 6, "Distinct input vertices were merged");
        vtkNew<vtkIdTypeArray> provenance;
        provenance->SetName("vtkOriginalPointIds");
        for (vtkIdType id : {0,1,2,1,0,3}) provenance->InsertNextValue(id);
        seam->GetPointData()->AddArray(provenance);
        auto joined = SharedPolygonEdges::build(seam, nullptr, provenance, 4);
        require(joined && joined->edgeCount() == 5, "Source seam not shared");
        std::set<Edge> seamIndices;
        joined->forEachBatch(seam, [&](const std::uint32_t* ids, std::size_t count) {
            for (std::size_t i = 0; i < count; i += 2) seamIndices.emplace(ids[i], ids[i + 1]);
        });
        require(seamIndices == std::set<Edge>{{0,1},{0,2},{1,2},{3,5},{4,5}}, "Drawing lost owning patch indices");
        provenance->SetValue(5, -1);
        provenance->Modified();
        require(!joined->matches(seam) && !SharedPolygonEdges::build(seam, nullptr, provenance, 4),
                "Invalid or stale source IDs were accepted");
        vtkNew<vtkImageData> cube;
        cube->SetDimensions(2,2,2);
        const auto boundary = aerosim::visualization::BoundarySurfaceExtractor::extract(cube);
        require(boundary->sharedEdges && boundary->sharedEdges->edgeCount() == 12, "Voxel must have 12 boundary edges");

        vtkNew<vtkUnsignedCharArray> flags;
        flags->SetNumberOfTuples(8);
        mesh->GetPointData()->SetAttribute(flags, vtkDataSetAttributes::EDGEFLAG);
        require(!SharedPolygonEdges::build(mesh), "Explicit edge flags require native rendering");
        require(!SharedPolygonEdges::build(surface(3, {{0,1,3}})), "Invalid endpoint accepted");
        require(!SharedPolygonEdges::build(surface(3, {{0,1,1}})), "Collapsed edge accepted");
        auto mixed = surface(3, {{0,1,2}});
        vtkNew<vtkCellArray> lines;
        const vtkIdType segment[] = {0,1};
        lines->InsertNextCell(2, segment);
        mixed->SetLines(lines);
        require(!SharedPolygonEdges::build(mixed), "Mixed primitives must use native rendering");

        // A shuffled high-valence fan crosses the upload batch boundary.
        constexpr unsigned rim = 18001;
        std::vector<std::vector<vtkIdType>> faces;
        std::set<Edge> fanEdges;
        for (unsigned i = 0; i < rim; ++i) {
            const auto j = (i * 17) % rim;
            const auto a = j + 1, b = (j + 1) % rim + 1;
            faces.push_back({0, a, b});
            fanEdges.emplace(0, a);
            fanEdges.emplace((std::min)(a, b), (std::max)(a, b));
        }
        checkEdges(surface(rim + 1, faces), fanEdges);
        std::atomic_bool cancel{true};
        bool cancelled = false;
        try { SharedPolygonEdges::build(mesh, &cancel); }
        catch (const std::runtime_error&) { cancelled = true; }
        require(cancelled, "Cancelled request was processed");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
