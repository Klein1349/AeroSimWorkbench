add_library(aerosim_application STATIC ApplicationController.cpp)
add_library(aerosim::application ALIAS aerosim_application)
target_include_directories(aerosim_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aerosim_application PUBLIC aerosim_contracts Qt5::Core)

add_executable(aerosim_cli cli_main.cpp)
target_link_libraries(aerosim_cli PRIVATE aerosim_application aerosim_mock Qt5::Core)
set_target_properties(aerosim_cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(AEROSIM_BUILD_GUI)
    # A desktop GUI must not allocate a second console window on Windows.
    add_executable(aerosim_gui WIN32 gui_main.cpp)
    target_link_libraries(aerosim_gui PRIVATE
        aerosim_application aerosim_mock aerosim_ui Qt5::Widgets)
    # Inject the optional native boundary viewer into the existing UI host.
    if(TARGET aerosim_visualization_scene)
        target_sources(aerosim_gui PRIVATE BoundaryViewerBinding.cpp)
        target_link_libraries(aerosim_gui PRIVATE aerosim_visualization_scene)
        target_compile_definitions(aerosim_gui PRIVATE AEROSIM_HAS_VTK_VIEWER=1)
    endif()
    set_target_properties(aerosim_gui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    # The standalone workbench has its own EXE and deployment directory so a
    # running main window does not lock its binary or its Qt runtime files.
    add_executable(aerosim_postprocess WIN32 gui_main.cpp)
    target_link_libraries(aerosim_postprocess PRIVATE
        aerosim_application aerosim_mock aerosim_ui Qt5::Widgets)
    target_compile_definitions(aerosim_postprocess PRIVATE AEROSIM_POSTPROCESS_STANDALONE=1)
    if(TARGET aerosim_visualization_scene)
        target_sources(aerosim_postprocess PRIVATE BoundaryViewerBinding.cpp)
        target_link_libraries(aerosim_postprocess PRIVATE aerosim_visualization_scene)
        target_compile_definitions(aerosim_postprocess PRIVATE AEROSIM_HAS_VTK_VIEWER=1)
    endif()
    set_target_properties(aerosim_postprocess PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/postprocess")
    if(TARGET aerosim_visualization_scene AND AEROSIM_BUILD_TESTS)
        add_executable(aerosim_boundary_gui_tests tests/BoundaryIntegrationTests.cpp BoundaryViewerBinding.cpp)
        target_include_directories(aerosim_boundary_gui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(aerosim_boundary_gui_tests PRIVATE aerosim_ui aerosim_visualization_scene)
        target_compile_definitions(aerosim_boundary_gui_tests PRIVATE
            "AEROSIM_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/postprocess\"")
        add_test(NAME boundary_gui_integration COMMAND aerosim_boundary_gui_tests)
        set_tests_properties(boundary_gui_integration PROPERTIES LABELS "native-opengl" TIMEOUT 90)
    endif()
endif()
