# B-Rep mesh boundary display

This feature implements discrete mesh boundary extraction for display. It does
not reconstruct CAD surfaces or export STEP. Input files are `.vtk`, `.vtu`,
`.vti` and `.vtp`. Supported data are full three-dimensional ImageData,
linear tetrahedron/hexahedron/voxel/wedge/pyramid UnstructuredGrid, and
triangle/quad PolyData. Other datasets, higher-order cells, mixed dimensions,
nonzero ghost flags and unsupported polyhedra produce a visible error.

## Architecture and compatibility

`aerosim_boundary` contains the reader, geometry validation, surface extractor
and Qt Core asynchronous service. `aerosim_visualization_scene` exclusively owns
VTK rendering. `app/BoundaryViewerBinding.cpp` composes these with existing UI
signals and presentation endpoints. No shared `contracts/` header or virtual
interface changes. A one-line `QString::KeepEmptyParts` compatibility correction
in the existing resource implementation makes the latest baseline compile on
Qt 5.12, retaining its original path validation behavior.

Each loaded object has a unique ID; the service owns the original dataset and
immutable surface snapshot. A single worker owns its reader/filter until
completion. Only lightweight notification signals cross threads. Results are
consumed on the GUI thread after the worker has finished. Removing a pending
object cancels it, checks VTK progress when available, and prevents late results
from reappearing. Shutdown waits for the worker before destroying storage.

`vtkDataSetSurfaceFilter` generates `vtkOriginalPointIds` and
`vtkOriginalCellIds`; fields are retained with their input associations. These
are local input indices, not solver global IDs or boundary face IDs. Reserved
name collisions are rejected. Original body data remain available for later
analysis; changing display properties reuses the surface and does not rerun
extraction. Removing the object releases both scene and service references.

## User flow

With visualization compiled, both the main mesh-management viewport and the
existing post-processing workbench use the real viewer by default. Set
`AEROSIM_ENABLE_VTK_VIEWER=0` for the previous UI-only path; `--smoke` also skips
OpenGL. UI-only builds retain their explicitly labelled Mock examples.

In the main window, enter mesh management and use **添加网格** to choose a file.
The entry shows loading status until a validated boundary is available. Use
display properties for visibility, surface/wireframe/surface-with-edges,
color, opacity and display scale. Scale does not alter source coordinates;
unit labels do not perform unit conversion. Remove cancels a pending import
or releases a completed object. The status reports input counts and boundary
faces, unique source nodes, and actual surface point entries separately.

In the post-processing workbench, **导入结果** reads one mesh at a time.
Successful imports appear in both data and object lists. **对象样式** changes
the real object's visibility, edges, color and opacity; the object list can
remove it. Double-clicking a boundary object opens its style editor. Camera
and background controls use the real renderer. Unconnected
analysis, export and sample-data actions are disabled in boundary mode.
The time slider remains disabled: files are independent mesh snapshots, not an
implemented time-series result session. Use left drag to rotate, middle drag
to pan and the wheel to zoom.

## Boundaries and limits

The first version extracts **all topological boundaries**. It does not infer
wall/inlet/outlet labels, select only physical walls, stitch partition seams,
merge coincident nodes or simplify surfaces. Existing point/cell fields pass
through, but solver FaceData is not reconstructed from volume CellData.
Input coordinates must be finite; connectivity and repeated/nonmanifold
volume faces are checked. General geometric quality and self-intersection
checks are not part of this implementation.

No universal “90% smaller” claim is made. Structured n³-cell cube data produce
6n² boundary quads; a million-cell cube produces 60,000 quads. Counts of volume
cells and faces are different measures. Structured VTK surfaces may duplicate
seam points. Original data plus cached surfaces remain in memory, so reduced
display geometry does not imply reduced total process memory. VTK's ordinary
DataSetMapper already extracts surfaces internally.

## Build and validation

Prepare the exact dependencies described in `Dependencies.md`. Set the existing
`AEROSIM_QT_ROOT`, `AEROSIM_SARIBBON_SOURCE_DIR`, `AEROSIM_VTK_DIR`,
`AEROSIM_VTK_BIN` and `AEROSIM_BUILD_DIR` environment variables to local paths.

Independent geometry/service target (no Qt Widgets or SARibbon required):

```bat
cmake -S modules/visualization -B build-boundary-core -G "Visual Studio 15 2017 Win64" -DCMAKE_PREFIX_PATH="%AEROSIM_QT_ROOT%" -DVTK_DIR="%AEROSIM_VTK_DIR%" -DAEROSIM_BUILD_GUI=OFF
cmake --build build-boundary-core --config Debug
cd build-boundary-core
ctest -C Debug --output-on-failure
```

Full application, from the repository root in a VS2017 developer environment:

```bat
set AEROSIM_BUILD_VISUALIZATION=ON
scripts\build-windows.cmd all Debug
scripts\run-postprocess-windows.cmd
```

`boundary_geometry` checks shared faces, provenance/fields, outward winding and
area of the analytical cube, large structured counts, input rejection, readers,
async handoff and cancellation. `boundary_gui_integration` requires native
OpenGL; it drives the actual file dialogs in both existing windows and checks
real scene geometry, independent styles and removal. It is not an offscreen
Mock test. Set `AEROSIM_BOUNDARY_CAPTURE_DIR` to save GUI evidence.

Verified on the local Windows x64 Debug environment on 2026-11-07, based on
`origin/main` commit `35c3bab4f6d1a5e29a1f1a4f5ff493fc80d93d29`:

- Independent geometry/service build and its CTest passed without Qt Widgets.
- Full application build passed; all 17 CTests passed, including the native
  OpenGL GUI test (actual file dialogs, real actors in both existing windows).
- Analytical fixture: 27 input points / 8 body cells, 24 boundary quads,
  26 unique source nodes, 54 VTK surface point entries, area 24, outward winding.
- Million-cell structured fixture: 60,000 boundary quads and 60,002 unique
  source nodes; this is a topology check, not a peak-memory benchmark.
- Shared tetra/voxel faces, hexahedron/wedge/pyramid, all four file extensions,
  point/cell field provenance, corrupt input, duplicate/nonmanifold cells,
  ghost rejection, worker handoff, cancellation and resource release passed.
- GUI screenshots were inspected. This caught and fixed an initial tree-item
  signal hiding imported meshes. Assertions now require imported and remaining
  objects to stay visible. Real-mode example labels, timeline controls,
  background settings and object selection summaries were synchronized.

The local test/application directories contain matching Qt/VTK/Debug CRT DLLs.
With CMake 3.18 on this machine, changing per-test environment variables can
corrupt non-ASCII DLL search paths. Deploying DLLs beside executables resolves
this; for Qt tools and capture paths, use an ASCII dependency path or temporary
`subst` alias. This does not require a compiler or library upgrade. The separate
target-machine acceptance on the project's designated Windows host has not
been performed.

## Constitution compliance

Dependency direction and UI-only presentation responsibilities are preserved.
Services have no QWidget/renderer dependency; VTK arrays stay module-local.
Public contracts are unchanged. Cross-thread completion uses queued lightweight
notifications with explicit ownership and cancellation. Independent and full
build checks are separate; Mock tests are not numerical or rendering evidence.
The existing SARibbon version discrepancy is recorded, not silently changed.
