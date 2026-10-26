# Optional full-stack check; shares SARibbon with the UI target.
include("${CMAKE_CURRENT_LIST_DIR}/../SARibbonDependency.cmake")
find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Widgets OpenGL Svg)
find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS
    vtkCommonCore vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2)

include("${VTK_USE_FILE}")
add_executable(aerosim_stack_probe StackProbe.cpp)
target_link_libraries(aerosim_stack_probe PRIVATE
    Qt5::Widgets Qt5::OpenGL SARibbonBar::SARibbonBar ${VTK_LIBRARIES})
if(MSVC)
    get_target_property(_vtk_configs vtkGUISupportQt IMPORTED_CONFIGURATIONS)
    if("DEBUG" IN_LIST _vtk_configs AND NOT "RELEASE" IN_LIST _vtk_configs)
        message(STATUS "VTK vtkGUISupportQt provides Debug only; Release stack probe will fail at compile time")
        target_compile_definitions(aerosim_stack_probe PRIVATE
            "$<$<NOT:$<CONFIG:Debug>>:AEROSIM_VTK_DEBUG_ONLY>")
    endif()
endif()
if(AEROSIM_BUILD_TESTS)
    add_test(NAME stack_probe COMMAND aerosim_stack_probe)
endif()
