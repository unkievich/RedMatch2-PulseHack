cmake_minimum_required(VERSION 3.21)

project(PulseHack VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")

set(PULSE_HACK_SOURCES
    include/pulse/plugin.h
    src/main.cpp
    src/features/aimbot.cpp
    src/features/aimbot.h
    src/features/esp.cpp
    src/features/esp.h
    src/features/movement.cpp
    src/features/movement.h
    src/ui/blur.cpp
    src/ui/blur.h
    src/ui/pulse_ui.cpp
    src/ui/pulse_ui.h
    src/ui/assets/font/fonts.h
    src/ui/assets/font/icons.h
    src/ui/assets/font/mingcute_icon_font.h
    src/ui/assets/logo/pulse_logo.h
)

add_library(pulse_hack SHARED
    ${PULSE_HACK_SOURCES}
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(pulse_hack
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_compile_definitions(pulse_hack PRIVATE
    RM2_PLUGIN_BUILD
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(pulse_hack PRIVATE /W4 /permissive-)
endif()

target_link_libraries(pulse_hack PRIVATE
    d3d11
    d3dcompiler
    dxgi
    dwmapi
)

# Keep the injected DLL self-contained when building with LLVM-MinGW. The
# target process cannot resolve libc++.dll/libunwind.dll from our toolchain.
if(MINGW)
    target_link_options(pulse_hack PRIVATE -static)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PULSE_HACK_SOURCES})

set_target_properties(pulse_hack PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pulse-hack"
    PDB_NAME "pulse-hack"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
