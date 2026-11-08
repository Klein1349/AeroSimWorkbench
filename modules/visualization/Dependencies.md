# Boundary display dependencies

The boundary implementation uses the existing C++14 / Qt 5.12.12 / VTK 8.2.0
baseline. No JSON library, CAD kernel, Python runtime or solver is introduced.

| Dependency | Source and configuration |
| --- | --- |
| VTK | Kitware/VTK tag `v8.2.0`, commit `e3de2c35c9f44fd6d16ad4c6b6527de7c4f677c7`; BSD license in upstream `Copyright.txt` |
| CMake package | `VTK` 8.2.0 EXACT; legacy `vtkCommonCore`, `vtkFiltersGeometry`, `vtkIOLegacy`, `vtkIOXML`, `vtkGUISupportQt`, rendering and interaction targets |
| Compiler | VS2017 v141, MSVC 19.16.27054, x64, Debug shared VTK libraries, `/MDd` |
| Qt | 5.12.12 `msvc2017_64`, Core; scene additionally Widgets/OpenGL |
| GUI host | Existing repository SARibbon dependency unchanged; current CMake locks 2.9.2, despite historical prose naming 2.9.3 |

The local VTK build uses CMake 3.18.2 and only the requested modules plus their
dependencies, with tests/examples/wrappers disabled. VTK's original
`CMake/vtkCompilerPlatformFlags.cmake:125` has an invalid CACHE declaration.
The local dependency source uses this build-only correction:

```diff
- set(CMAKE_CXX_MP_NUM_PROCESSORS CACHE ${PROCESSOR_COUNT} "The maximum number of processes for the /MP flag")
+ set(CMAKE_CXX_MP_NUM_PROCESSORS "${PROCESSOR_COUNT}" CACHE STRING "The maximum number of processes for the /MP flag")
```

This changes no VTK runtime code or ABI. It is required when reproducing this
source build; an existing compatible VTK installation needs no modification.
Qt 5.12's `moc` and `windeployqt` also require ASCII path aliases in this local
Chinese directory. The build's generated moc response files used a temporary
`subst` alias for source/include/output paths; installed library paths remain
the real configured paths. Prefer an ASCII dependency root when reproducing
the source build. No generated dependency projects are committed.
Machine paths, DLLs, source downloads and build outputs are outside versioned
source. Release is rejected for this integration, matching the approved Debug
baseline. Validation results are recorded in `BoundaryDisplay.md`.
