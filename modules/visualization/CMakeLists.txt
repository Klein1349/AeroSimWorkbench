cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimVisualization LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_AUTOMOC ON)
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
    option(AEROSIM_BUILD_GUI "Build the VTK viewer" ON)
    option(AEROSIM_BUILD_TESTS "Build boundary tests" ON)
    if(AEROSIM_BUILD_TESTS)
        enable_testing()
    endif()
endif()

find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)
find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS vtkCommonCore vtkCommonDataModel
    vtkCommonExecutionModel vtkFiltersGeometry vtkIOLegacy vtkIOXML)
add_library(aerosim_boundary STATIC
    src/features/BoundarySurfaceExtractor.cpp src/data/BoundaryMeshReader.cpp
    src/data/BoundaryLoadService.cpp include/aerosim/visualization/data/BoundaryLoadService.h)
target_include_directories(aerosim_boundary PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(aerosim_boundary SYSTEM PUBLIC ${VTK_INCLUDE_DIRS})
target_link_libraries(aerosim_boundary PUBLIC Qt5::Core vtkCommonCore vtkCommonDataModel
    PRIVATE vtkCommonExecutionModel vtkFiltersGeometry vtkIOLegacy vtkIOXML)
if(MSVC)
    target_compile_options(aerosim_boundary PRIVATE /utf-8)
    target_compile_definitions(aerosim_boundary PRIVATE "$<$<NOT:$<CONFIG:Debug>>:AEROSIM_VTK_DEBUG_ONLY>")
endif()
if(AEROSIM_BUILD_GUI)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Widgets OpenGL)
    find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS
        vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2 vtkInteractionStyle)
    add_library(aerosim_visualization_scene STATIC
        src/scene/VtkSceneViewer.cpp include/aerosim/visualization/scene/VtkSceneViewer.h)
    target_include_directories(aerosim_visualization_scene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_include_directories(aerosim_visualization_scene SYSTEM PUBLIC ${VTK_INCLUDE_DIRS})
    target_link_libraries(aerosim_visualization_scene PUBLIC aerosim_boundary Qt5::Widgets
        PRIVATE Qt5::OpenGL vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2 vtkInteractionStyle)
    if(MSVC)
        target_compile_options(aerosim_visualization_scene PRIVATE /utf-8)
    endif()
endif()
if(AEROSIM_BUILD_TESTS)
    add_executable(aerosim_boundary_tests tests/BoundaryTests.cpp)
    target_link_libraries(aerosim_boundary_tests PRIVATE aerosim_boundary vtkIOXML)
    target_compile_definitions(aerosim_boundary_tests PRIVATE
        "AEROSIM_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/postprocess\"")
    add_test(NAME boundary_geometry COMMAND aerosim_boundary_tests)
endif()
