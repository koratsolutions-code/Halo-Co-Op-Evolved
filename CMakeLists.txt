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

add_library(minhook STATIC
    deps/minhook/src/buffer.c
    deps/minhook/src/hook.c
    deps/minhook/src/trampoline.c
    deps/minhook/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC deps/minhook/include)

# Encabezados de NVENC/NVDEC (las DLL las trae el driver de NVIDIA).
add_library(nvcodec INTERFACE)
target_include_directories(nvcodec INTERFACE deps/nv-codec-headers/include)

# Mod dentro del juego (VERSION.dll en mcc\binaries\win64): captura y codifica los cuadrantes de J2-J4.
add_library(HaloCoopGame SHARED
    src/game/dllmain.cpp
    src/game/proxy_version.cpp
    src/game/video.cpp
    src/game/encoder.cpp
    src/game/log.cpp
    src/game/version.def
)
set_target_properties(HaloCoopGame PROPERTIES OUTPUT_NAME VERSION)
target_link_libraries(HaloCoopGame PRIVATE minhook nvcodec d3d11 dxgi ws2_32)

# Host: crea los mandos de J2-J4, recibe su input por red y muestra el panel.
add_executable(HaloCoopHost WIN32
    src/launcher/main.cpp
    src/launcher/pads.cpp
    src/launcher/net_server.cpp
    src/launcher/game_install.cpp
    src/launcher/log.cpp
)
target_link_libraries(HaloCoopHost PRIVATE ViGEmClient setupapi xinput shell32 ws2_32 iphlpapi advapi32)

# Cliente: lo usan los jugadores 2, 3 y 4.
add_executable(HaloCoopClient WIN32
    src/client/main.cpp
    src/client/input.cpp
    src/client/net_client.cpp
    src/client/settings.cpp
    src/client/decoder.cpp
    src/client/renderer.cpp
    src/client/log.cpp
)
target_link_libraries(HaloCoopClient PRIVATE imgui nvcodec d3d11 dxgi d3dcompiler xinput ws2_32 winmm)

# Solo para pruebas: "juego" falso que carga el mod (no se distribuye).
add_executable(HaloCoopTestGame WIN32 tools/test_game.cpp)
target_link_libraries(HaloCoopTestGame PRIVATE d3d11 dxgi)
