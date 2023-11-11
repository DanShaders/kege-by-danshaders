# FIXME: This can probably be lowered.
cmake_minimum_required(VERSION 3.27)

project(KEGE VERSION 0.0.0)

include(ExternalProject)
include(FetchContent)
include(FindPkgConfig)

# FIXME: At least on Arch Linux, /usr/lib/cmake is not in a default search path. Figure out whose
#        responsibility is to add it.
find_package(protobuf/protobuf REQUIRED CONFIG PATHS /usr/lib/cmake)

include(common_compile_options.cmake)

get_filename_component(KEGE_PROJECT_ROOT "${PROJECT_SOURCE_DIR}/../.." ABSOLUTE CACHE)

set(CMAKE_EXPORT_COMPILE_COMMANDS On)

add_subdirectory("${KEGE_PROJECT_ROOT}/tools/cpp-protoc-initializers" "${CMAKE_BINARY_DIR}/cpp-protoc-initializers")
add_subdirectory("${KEGE_PROJECT_ROOT}/tools/sql-typer" "${CMAKE_BINARY_DIR}/sql-typer")
add_subdirectory("${KEGE_PROJECT_ROOT}/api" "${CMAKE_BINARY_DIR}/api")
