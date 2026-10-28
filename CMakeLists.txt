cmake_minimum_required(VERSION 3.20)
project(halo1_coop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # sin depender del runtime de VC++
add_compile_options(/utf-8 /W3 /wd4828)
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WINSOCK_DEPRECATED_NO_WARNINGS)

add_subdirectory(deps/ViGEmClient EXCLUDE_FROM_ALL)

add_library(imgui STATIC
    deps/imgui/imgui.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/backends/imgui_impl_win32.cpp
    deps/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC deps/imgui deps/imgui/backends)

# Host: crea los mandos de J2-J4, recibe su input por red y muestra el panel.
add_executable(HaloCoopHost WIN32
    src/launcher/main.cpp
    src/launcher/pads.cpp
    src/launcher/net_server.cpp
    src/launcher/log.cpp
)
target_link_libraries(HaloCoopHost PRIVATE ViGEmClient setupapi xinput shell32 ws2_32 iphlpapi)

# Cliente: lo usan los jugadores 2, 3 y 4.
add_executable(HaloCoopClient WIN32
    src/client/main.cpp
    src/client/input.cpp
    src/client/net_client.cpp
    src/client/settings.cpp
)
target_link_libraries(HaloCoopClient PRIVATE imgui d3d11 dxgi xinput ws2_32 winmm)
