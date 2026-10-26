add_library(aerosim_ui STATIC
    PostProcessWorkbenchWindow.cpp
    include/aerosim/ui/PostProcessWorkbenchWindow.h)
add_library(aerosim::ui ALIAS aerosim_ui)

target_include_directories(aerosim_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(aerosim_ui
    PUBLIC
        aerosim_contracts
        Qt5::Widgets)
if(MSVC)
    target_compile_options(aerosim_ui PRIVATE /utf-8)
endif()

add_executable(aerosim_postprocess_ui_smoke
    tests/PostProcessWorkbenchWindowSmoke.cpp)
target_link_libraries(aerosim_postprocess_ui_smoke PRIVATE aerosim_ui Qt5::Widgets)
set_target_properties(aerosim_postprocess_ui_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
