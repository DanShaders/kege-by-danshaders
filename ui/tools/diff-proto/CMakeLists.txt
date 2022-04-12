cmake_minimum_required(VERSION 3.10)

project(diff-proto VERSION 0.0.0)

include(FindProtobuf)
find_package(Protobuf REQUIRED)

include(FindPkgConfig)
pkg_check_modules(FMT REQUIRED fmt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(
	CXX_SOURCES
	src/fields-id.cc
	src/fields-scalar.cc
	src/fields-set.cc
	src/fields-unwired.cc
	src/main.cc
	src/options.cc
)

set(EXE_NAME diff-proto)

add_executable(${EXE_NAME} ${CXX_SOURCES})

target_precompile_headers(
	${EXE_NAME} PRIVATE
	src/stdafx.h
)

target_include_directories(
	${EXE_NAME} PRIVATE
	${Protobuf_INCLUDE_DIRS}
	${FMT_INCLUDE_DIRS}
)

target_compile_options(
	${EXE_NAME} PRIVATE
	-Wall -Wextra -Wshadow -Wconversion
	-fdiagnostics-color=always
)

target_link_libraries(
	${EXE_NAME} PRIVATE
	${Protobuf_PROTOC_LIBRARIES}
	${Protobuf_LIBRARIES}
	${FMT_LIBRARIES}
)

if (CMAKE_BUILD_TYPE STREQUAL Release)
	set_property(TARGET ${EXE_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")

install(
	TARGETS ${EXE_NAME}
	CONFIGURATIONS Release
	RUNTIME DESTINATION bin
)

find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
	file(
		GLOB_RECURSE ALL_CXX_SOURCE_FILES
		*.cc *.h
	)
	add_custom_target(
		format
		COMMAND ${CLANG_FORMAT} -i ${ALL_CXX_SOURCE_FILES}
	)
endif()
