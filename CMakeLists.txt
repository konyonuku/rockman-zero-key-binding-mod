cmake_minimum_required(VERSION 3.15.0)
project(KeyRebind VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(KeyRebind SHARED
    src/dllmain.cpp
    src/arc.h src/arc.cpp
    src/atlas.h src/atlas.cpp
    src/config.h src/config.cpp
    src/gui.h src/gui.cpp
    src/icons.h src/icons.cpp
    src/input_hook.h src/input_hook.cpp
    src/keycaps.h src/keycaps.cpp
    src/keys.h src/keys.cpp
    src/loader.h src/loader.cpp
    src/log.h src/log.cpp
    src/remap.h src/remap.cpp
    src/slots.h src/slots.cpp
    src/strings.h src/strings.cpp
    src/third_party/miniz.c
)

target_compile_definitions(KeyRebind PRIVATE
    MINIZ_NO_STDIO
    MINIZ_NO_TIME
    MINIZ_NO_ARCHIVE_APIS
)

target_compile_options(KeyRebind PRIVATE /utf-8)

target_link_libraries(KeyRebind PRIVATE
    dxguid
)

add_executable(KeyRebindConfig WIN32
    src/main_config.cpp
    src/config.h src/config.cpp
    src/gui.h src/gui.cpp
    src/keys.h src/keys.cpp
    src/strings.h src/strings.cpp
)

target_compile_options(KeyRebindConfig PRIVATE /utf-8)
