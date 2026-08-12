cmake_minimum_required(VERSION 3.15.0)
project(KeyRebind VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(KeyRebind SHARED
    src/dllmain.cpp
    src/config.h src/config.cpp
    src/input_hook.h src/input_hook.cpp
    src/keys.h src/keys.cpp
    src/log.h src/log.cpp
    src/remap.h src/remap.cpp
)

target_link_libraries(KeyRebind PRIVATE
    dxguid
)
