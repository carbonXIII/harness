cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(harness)

add_subdirectory(external/fmt)
include(cmake/AddMultiTargetComponent.cmake)

add_subdirectory_for_toolchain(host "x86_64")
add_subdirectory_for_toolchain(pi "armv6l")

# trigger_toolchain_build("rpi" "x86_64")
