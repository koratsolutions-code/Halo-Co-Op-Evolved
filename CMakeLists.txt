cmake_minimum_required(VERSION 3.20)
project(halo1_coop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # sin depender del runtime de VC++
add_compile_options(/utf-8 /W3 /wd4828)
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

add_subdirectory(deps/ViGEmClient EXCLUDE_FROM_ALL)

add_executable(HaloCoopHost WIN32
    src/launcher/main.cpp
    src/launcher/pads.cpp
    src/launcher/log.cpp
)
target_link_libraries(HaloCoopHost PRIVATE ViGEmClient setupapi xinput shell32)
