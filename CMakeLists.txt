cmake_minimum_required(VERSION 3.25)
project(engine_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(projects/app_hello)
add_subdirectory(projects/app_math)
add_subdirectory(projects/phase_00_engine_bootstrap)
add_subdirectory(projects/phase_01_tilemap_rendering)
