find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core Widgets OpenGL)
find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS
    vtkCommonCore vtkCommonDataModel vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2)

foreach(probe IN ITEMS import_cases lifecycle_probe)
    if(probe STREQUAL "lifecycle_probe")
        set(source lifecycle.cpp)
    else()
        set(source import_cases.cpp)
    endif()
    add_executable(${probe} ${source})
    target_compile_features(${probe} PRIVATE cxx_std_14)
    target_compile_options(${probe} PRIVATE /utf-8)
    target_link_libraries(${probe} PRIVATE aerosim_mesh_boundary Qt5::Core Psapi)
    set_target_properties(${probe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endforeach()

add_executable(ui_import_only ui_import_only.cpp "${PROJECT_SOURCE_DIR}/app/MeshViewerBinding.cpp")
target_compile_features(ui_import_only PRIVATE cxx_std_14)
target_compile_options(ui_import_only PRIVATE /utf-8)
target_include_directories(ui_import_only PRIVATE "${PROJECT_SOURCE_DIR}/app")
target_link_libraries(ui_import_only PRIVATE aerosim_ui aerosim_mesh_scene Qt5::OpenGL
    vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2 vtkCommonDataModel Psapi)
set_target_properties(ui_import_only PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
