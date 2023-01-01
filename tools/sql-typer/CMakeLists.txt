cmake_minimum_required(VERSION 3.10)

project(sql-typer VERSION 0.0.0)

include(FindPkgConfig)
pkg_check_modules(LIBPQ REQUIRED libpq)
pkg_check_modules(FMT REQUIRED fmt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(
	CXX_SOURCES
	main.cc
)

set(EXE_NAME sql-typer)

add_executable(${EXE_NAME} ${CXX_SOURCES})

target_compile_options(
	${EXE_NAME} PRIVATE
	-Wall -Wextra -Wshadow -Wconversion
	-Wno-missing-field-initializers
	-fdiagnostics-color=always
)

target_include_directories(
	${EXE_NAME} PRIVATE
	${LIBPQ_INCLUDE_DIRS}
	${FMT_INCLUDE_DIRS}
)

target_link_libraries(
	${EXE_NAME} PRIVATE
	${LIBPQ_LIBRARIES}
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
find_program(PG_FORAMT "pg-format")

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
