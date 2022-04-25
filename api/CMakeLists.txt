cmake_minimum_required(VERSION 3.10)

project(KEGE VERSION 0.0.0)

include(FindPkgConfig)
include(ExternalProject)
include(FetchContent)
include(FindProtobuf)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (CMAKE_BUILD_TYPE STREQUAL Release)
	set(KEGE_EXCEPTION_STACKTRACE 0)
	set(KEGE_LOG_DEBUG_ENABLED 0)
else()
	set(KEGE_EXCEPTION_STACKTRACE 1)
	set(KEGE_LOG_DEBUG_ENABLED 1)
endif()
configure_file(include/KEGE.in.h KEGE.h)


# ==== Sources ====
set(
	KEGE_BASE_SOURCES
	src/async/coro.cc
	src/async/curl.cc
	src/async/event-loop.cc
	src/async/future.cc
	src/async/libev-event-loop.cc
	src/async/mutex.cc
	src/async/pq.cc
	src/async/socket.cc
	src/async/stackful.cc
	src/config.cc
	src/fcgx.cc
	src/logging.cc
	src/routes.cc
	src/routes/session.cc
	src/stacktrace.cc
	src/stacktrace-specific.cc
	src/utils/api.cc
	src/utils/common.cc
	src/utils/crypto.cc

	routes/_basic.cc
	routes/_wrap.cc
	routes/task-types.cc
	routes/tasks.cc
	routes/user.cc
)

set(
	KEGE_SOURCES
	${KEGE_BASE_SOURCES}
	src/main.cc
)

set(
	PROTO_SOURCES
	../proto/api.proto
	../proto/diff.proto
	../proto/task-types.proto
	../proto/tasks.proto
	../proto/user.proto
)


# ==== Target libbacktrace ====
if (KEGE_EXCEPTION_STACKTRACE)
	ExternalProject_Add(
		LIBBACKTRACE
		GIT_REPOSITORY "https://github.com/ianlancetaylor/libbacktrace.git"
		GIT_TAG "4d2dd0b172f2c9192f83ba93425f868f2a13c553"
		CONFIGURE_COMMAND
			<SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
		UPDATE_COMMAND ""
	)
	ExternalProject_Get_property(LIBBACKTRACE INSTALL_DIR)
	set(BACKTRACE_INCLUDE_DIRS "${INSTALL_DIR}/include")
	set(BACKTRACE_LIBRARIES "${INSTALL_DIR}/lib/libbacktrace.a")
else()
	set(BACKTRACE_INCLUDE_DIRS "")
	set(BACKTRACE_LIBRARIES "")
endif()


# ==== Target cpp-protoc-initializers ====
ExternalProject_Add(
	CPP_PROTOC_INITIALIZERS
	GIT_REPOSITORY "https://github.com/DanShaders/cpp-protoc-initializers.git"
	GIT_TAG "6400cd88a27f78a7ed6b8d2efbc155a6d7be4600"
	CMAKE_ARGS
		-DCMAKE_BUILD_TYPE=Release
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
	EXCLUDE_FROM_ALL
	UPDATE_COMMAND ""
)
ExternalProject_Get_property(CPP_PROTOC_INITIALIZERS INSTALL_DIR)
set(CPP_PROTOC_INITIALIZERS_PATH "${INSTALL_DIR}/bin")


# ==== *.proto -> *.pb.cc ====
find_package(Protobuf REQUIRED)

set(proto_cxx_all)
protobuf_generate(
	APPEND_PATH OUT_VAR proto_cxx_all
	LANGUAGE cpp_initializers
	GENERATE_EXTENSIONS .pb.h .pb.cc
	PLUGIN protoc-gen-cpp_initializers=${CPP_PROTOC_INITIALIZERS_PATH}/protoc-gen-cpp_initializers
	PROTOS ${PROTO_SOURCES}
)
foreach(_file ${proto_cxx_all})
	if(_file MATCHES "cc$")
		list(APPEND PROTO_CXX_SRC ${_file})
	endif()
endforeach()


# ==== Target KEGE_BASE ====
add_library(KEGE_BASE INTERFACE)

add_dependencies(KEGE_BASE CPP_PROTOC_INITIALIZERS LIBBACKTRACE)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(FCGI REQUIRED fcgi)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(URIPARSER REQUIRED liburiparser)
pkg_check_modules(PQ REQUIRED libpq)


target_include_directories(
	KEGE_BASE INTERFACE
	"${PROJECT_BINARY_DIR}"
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${PROJECT_SOURCE_DIR}/include"
	
	${BACKTRACE_INCLUDE_DIRS}
	${CURL_INCLUDE_DIRS}
	${FCGI_INCLUDE_DIRS}
	${NLOHMANN_JSON_INCLUDE_DIRS}
	${OPENSSL_INCLUDE_DIRS}
	${URIPARSER_INCLUDE_DIRS}
	${PROTOBUF_INCLUDE_DIRS}
	${PQ_INCLUDE_DIRS}
)

target_compile_options(
	KEGE_BASE INTERFACE
	-Wall -Wextra -Wshadow -Wconversion -masm=intel
	-Wno-missing-field-initializers # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82283
	-fdiagnostics-color=always # https://github.com/ninja-build/ninja/issues/174
)

target_link_libraries(
	KEGE_BASE INTERFACE
	${BACKTRACE_LIBRARIES}
	${CURL_LIBRARIES}
	${FCGI_LIBRARIES}
	${NLOHMANN_JSON_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${URIPARSER_LIBRARIES}
	${PROTOBUF_LIBRARIES}
	${PQ_LIBRARIES}
	pthread
	dl
	ev
)

target_precompile_headers(
	KEGE_BASE INTERFACE
	include/stdafx.h
)


# ==== Target KEGE ====
add_executable(KEGE ${KEGE_SOURCES} ${PROTO_CXX_SRC})
target_link_libraries(KEGE PRIVATE KEGE_BASE)
if (CMAKE_BUILD_TYPE STREQUAL Release)
	set_property(TARGET KEGE PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()


# ==== Target format ====
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


# ==== Additional compiler options ====
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer -fsanitize=address,undefined -DKEGE_SANITIZE_ADDRESS")
set(CMAKE_CXX_FLAGS_DEBUGL "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_THREAD "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer -fsanitize=thread -DKEGE_SANITIZE_THREAD")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
