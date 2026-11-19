find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS OpenGL)
find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS
    vtkCommonCore vtkCommonDataModel vtkCommonExecutionModel
    vtkFiltersGeometry vtkIOLegacy vtkIOXML
    vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2 vtkInteractionStyle)

set(mesh_root "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(aerosim_mesh_boundary STATIC
    "${mesh_root}/src/data/BoundaryLoadService.cpp"
    "${mesh_root}/src/data/BoundaryMeshReader.cpp"
    "${mesh_root}/src/features/BoundarySurfaceExtractor.cpp"
    "${mesh_root}/src/features/SharedPolygonEdges.cpp"
    "${mesh_root}/include/aerosim/visualization/data/BoundaryLoadService.h")
target_include_directories(aerosim_mesh_boundary PUBLIC "${mesh_root}/include")
target_include_directories(aerosim_mesh_boundary SYSTEM PUBLIC ${VTK_INCLUDE_DIRS})
target_link_libraries(aerosim_mesh_boundary PUBLIC Qt5::Core vtkCommonCore vtkCommonDataModel
    PRIVATE vtkCommonExecutionModel vtkFiltersGeometry vtkIOLegacy vtkIOXML)
if(MSVC)
    target_compile_definitions(aerosim_mesh_boundary PRIVATE
        "$<$<NOT:$<CONFIG:Debug>>:AEROSIM_VTK_DEBUG_ONLY>")
endif()

set(AEROSIM_CGNS_ROOT "" CACHE PATH "Optional CGNS Debug installation or build prefix")
find_path(AEROSIM_CGNS_INCLUDE_DIR cgnslib.h
    HINTS "${AEROSIM_CGNS_ROOT}/include" "${AEROSIM_CGNS_ROOT}/src")
find_library(AEROSIM_CGNS_LIBRARY NAMES cgnsdll cgns
    HINTS "${AEROSIM_CGNS_ROOT}/lib" "${AEROSIM_CGNS_ROOT}/src/Debug")
if(AEROSIM_CGNS_INCLUDE_DIR AND AEROSIM_CGNS_LIBRARY)
    target_sources(aerosim_mesh_boundary PRIVATE "${mesh_root}/data/src/CgnsAdapter.cpp")
    target_include_directories(aerosim_mesh_boundary PRIVATE
        "${mesh_root}/data/include" "${AEROSIM_CGNS_INCLUDE_DIR}")
    target_link_libraries(aerosim_mesh_boundary PRIVATE "${AEROSIM_CGNS_LIBRARY}")
    target_compile_definitions(aerosim_mesh_boundary PUBLIC AEROSIM_MESH_HAS_CGNS=1)
    if(WIN32 AND AEROSIM_CGNS_LIBRARY MATCHES "cgnsdll")
        target_compile_definitions(aerosim_mesh_boundary PRIVATE USE_DLL)
    endif()
endif()

add_library(aerosim_mesh_scene STATIC
    "${mesh_root}/src/scene/BoundaryPolyDataMapper.cpp"
    "${mesh_root}/src/scene/VtkSceneViewer.cpp"
    "${mesh_root}/include/aerosim/visualization/scene/VtkSceneViewer.h")
target_link_libraries(aerosim_mesh_scene PUBLIC aerosim_mesh_boundary Qt5::Widgets
    PRIVATE Qt5::OpenGL vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2 vtkInteractionStyle)

if(AEROSIM_BUILD_TESTS)
    add_executable(aerosim_shared_polygon_edges_test "${mesh_root}/tests/SharedPolygonEdgesTest.cpp")
    target_link_libraries(aerosim_shared_polygon_edges_test PRIVATE aerosim_mesh_boundary)
    add_test(NAME mesh_shared_polygon_edges COMMAND aerosim_shared_polygon_edges_test)

    add_executable(aerosim_mesh_wheel_test "${mesh_root}/tests/MeshWheelZoomTest.cpp")
    target_link_libraries(aerosim_mesh_wheel_test PRIVATE aerosim_mesh_scene vtkGUISupportQt vtkRenderingCore)
    add_test(NAME mesh_viewport_wheel_zoom COMMAND aerosim_mesh_wheel_test)
    set_tests_properties(mesh_viewport_wheel_zoom PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(aerosim_mesh_boundary_tests "${mesh_root}/tests/BoundaryTests.cpp")
    target_link_libraries(aerosim_mesh_boundary_tests PRIVATE aerosim_mesh_boundary vtkIOXML)
    target_compile_definitions(aerosim_mesh_boundary_tests PRIVATE
        "AEROSIM_FIXTURE_DIR=\"${PROJECT_SOURCE_DIR}/tests/fixtures/postprocess\"")
    add_test(NAME mesh_boundary_geometry COMMAND aerosim_mesh_boundary_tests)
endif()
