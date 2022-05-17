cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(harness)

find_package(SDL2 REQUIRED)

add_subdirectory(external/fmt)

add_executable(harness main.cpp)
target_link_libraries(harness PUBLIC fmt SDL2)
