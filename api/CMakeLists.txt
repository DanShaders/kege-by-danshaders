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

set(
	SQL_TYPER_PATH
	${CMAKE_SOURCE_DIR}/../tools/sql-typer/build/sql-typer
	CACHE STRING "Path to sql-typer binary"
)

set(
	DB_CONNECT_STR
	"postgresql://kege:password@localhost:5432/kege"
	CACHE STRING "Connection string for database for sql-typer"
)


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
	src/config.cc
	src/fcgx.cc
	src/routes.cc
	src/routes/grading.cc
	src/routes/session.cc
	src/stacktrace.cc
	src/stacktrace-specific.cc
	src/utils/api.cc
	src/utils/common.cc
	src/utils/crypto.cc
	src/utils/filter.cc
	src/utils/html.cc

	routes/_basic.cc
	routes/_wrap.cc
	routes/attachment.cc
	routes/contestant.cc
	routes/jobs.cc
	routes/kims.cc
	routes/standings.cc
	routes/task-types.cc
	routes/tasks.cc
	routes/user.cc
)

set(
	KEGE_SQL_SOURCES
	routes/contestant.sql
	routes/kims.sql
	routes/standings.sql
)

list(APPEND KEGE_SOURCES
	${KEGE_BASE_SOURCES}
	src/main.cc
)

set(
	PROTO_SOURCES
	../proto/api.proto
	../proto/contestant.proto
	../proto/diff.proto
	../proto/jobs.proto
	../proto/kims.proto
	../proto/task-types.proto
	../proto/tasks.proto
	../proto/user.proto
)


# ==== Dealing with SQL sources ====
foreach(filename IN LISTS KEGE_SQL_SOURCES)
	list(APPEND KEGE_SOURCES
		${filename}.cc
	)
	add_custom_command(
		OUTPUT ${CMAKE_SOURCE_DIR}/${filename}.cc
		COMMAND ${SQL_TYPER_PATH} ${DB_CONNECT_STR} ${CMAKE_SOURCE_DIR}/${filename}
		DEPENDS
			${filename}
			../config/postgresql/db.sql
			../tools/sql-typer/build/sql-typer
	)
endforeach()


# ==== Target libbacktrace ====
if (KEGE_EXCEPTION_STACKTRACE)
	ExternalProject_Add(
		LIBBACKTRACE
		GIT_REPOSITORY "https://github.com/ianlancetaylor/libbacktrace.git"
		GIT_TAG "4d2dd0b172f2c9192f83ba93425f868f2a13c553"
		CONFIGURE_COMMAND
			<SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
		UPDATE_COMMAND ""
		EXCLUDE_FROM_ALL
		BUILD_BYPRODUCTS
			<INSTALL_DIR>/lib/libbacktrace.a
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
	GIT_TAG "1e9148ddc1748f6f74378d04dbce2f71d813ac71"
	CMAKE_ARGS
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		-DCMAKE_BUILD_TYPE=Release
	EXCLUDE_FROM_ALL
	UPDATE_COMMAND ""
)
ExternalProject_Get_property(CPP_PROTOC_INITIALIZERS INSTALL_DIR)
set(CPP_PROTOC_INITIALIZERS_PATH "${INSTALL_DIR}/bin")


# ==== Target lexbor ====
ExternalProject_Add(
	LEXBOR
	GIT_REPOSITORY "https://github.com/lexbor/lexbor.git"
	GIT_TAG "9c6915b98b0c33ac140bc85cc2c9b9eb0a93751b"
	CMAKE_ARGS
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_BUILD_TYPE=Release
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		-DLEXBOR_BUILD_SHARED=OFF
	UPDATE_COMMAND ""
	EXCLUDE_FROM_ALL
	BUILD_BYPRODUCTS
		<INSTALL_DIR>/lib/liblexbor_static.a
)
ExternalProject_Get_property(LEXBOR INSTALL_DIR)
set(LEXBOR_INCLUDE_DIRS "${INSTALL_DIR}/include")
set(LEXBOR_LIBRARIES "${INSTALL_DIR}/lib/liblexbor_static.a")


# ==== Target fmtlog ====
# Someone said "easy setup"?
ExternalProject_Add(
	FMTLOG
	DOWNLOAD_COMMAND
		rm -r <SOURCE_DIR> &&
		git clone https://github.com/MengRao/fmtlog.git <SOURCE_DIR> &&
		cd <SOURCE_DIR> &&
		git checkout dbb4bfe389b53c84a06fc9529ef57aa82afccb06 &&
		git submodule update --init ./fmt &&
		echo > ./bench/CMakeLists.txt &&
		cd -
	CMAKE_ARGS
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		-DCMAKE_BUILD_TYPE=Release
	UPDATE_COMMAND ""
	EXCLUDE_FROM_ALL
	BUILD_BYPRODUCTS
		<INSTALL_DIR>/lib/libfmtlog-static.a
	INSTALL_COMMAND
		cmake --install <BINARY_DIR> &&
		mkdir -p <INSTALL_DIR>/include/fmtlog &&
		cp <SOURCE_DIR>/fmtlog.h <INSTALL_DIR>/include/fmtlog
)
ExternalProject_Get_property(FMTLOG INSTALL_DIR)
set(FMTLOG_INCLUDE_DIRS "${INSTALL_DIR}/include")
set(FMTLOG_LIBRARIES "${INSTALL_DIR}/lib/libfmtlog-static.a")


# ==== Target uriparser ====
ExternalProject_Add(
	URIPARSER
	GIT_REPOSITORY "https://github.com/uriparser/uriparser.git"
	GIT_TAG "634b678fa858abf1d1ebc0634e96e9e29596e92a"
	CMAKE_ARGS
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		-DCMAKE_BUILD_TYPE=Release
		-DBUILD_SHARED_LIBS=OFF
		-DURIPARSER_BUILD_DOCS=OFF
		-DURIPARSER_BUILD_TESTS=OFF
	UPDATE_COMMAND ""
	EXCLUDE_FROM_ALL
	BUILD_BYPRODUCTS
		<INSTALL_DIR>/lib/liburiparser.a
)
ExternalProject_Get_property(URIPARSER INSTALL_DIR)
set(URIPARSER_INCLUDE_DIRS "${INSTALL_DIR}/include")
set(URIPARSER_LIBRARIES "${INSTALL_DIR}/lib/liburiparser.a")


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

add_dependencies(KEGE_BASE CPP_PROTOC_INITIALIZERS LIBBACKTRACE LEXBOR FMTLOG)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(FCGI REQUIRED fcgi)
pkg_check_modules(FMT REQUIRED fmt)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(PQ REQUIRED libpq)


target_include_directories(
	KEGE_BASE INTERFACE
	"${PROJECT_BINARY_DIR}"
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${PROJECT_SOURCE_DIR}/include"

	${BACKTRACE_INCLUDE_DIRS}
	${CURL_INCLUDE_DIRS}
	${FCGI_INCLUDE_DIRS}
	${FMT_INCLUDE_DIRS}
	${FMTLOG_INCLUDE_DIRS}
	${LEXBOR_INCLUDE_DIRS}
	${NLOHMANN_JSON_INCLUDE_DIRS}
	${OPENSSL_INCLUDE_DIRS}
	${PQ_INCLUDE_DIRS}
	${PROTOBUF_INCLUDE_DIRS}
	${URIPARSER_INCLUDE_DIRS}
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
	${FMT_LIBRARIES}
	${FMTLOG_LIBRARIES}
	${LEXBOR_LIBRARIES}
	${NLOHMANN_JSON_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${PQ_LIBRARIES}
	${PROTOBUF_LIBRARIES}
	${URIPARSER_LIBRARIES}
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
find_program(PG_FORMAT "pg_format")

if(CLANG_FORMAT AND PG_FORMAT)
	file(
		GLOB_RECURSE ALL_CXX_SOURCE_FILES
		*.cc *.h
	)
	foreach(filename IN LISTS KEGE_SQL_SOURCES)
		add_custom_command(
			OUTPUT ${CMAKE_SOURCE_DIR}/${filename}.formatted
			COMMAND ${PG_FORMAT} -i ${CMAKE_SOURCE_DIR}/${filename}
			DEPENDS ${filename}
		)
		list(APPEND PG_FORMAT_SOURCES ${CMAKE_SOURCE_DIR}/${filename}.formatted)
	endforeach()

	add_custom_target(
		format
		COMMAND ${CLANG_FORMAT} -i ${ALL_CXX_SOURCE_FILES}
		DEPENDS ${PG_FORMAT_SOURCES}
	)
endif()


# ==== Additional compiler options ====
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize=alignment -DKEGE_SANITIZE_ADDRESS")
set(CMAKE_CXX_FLAGS_DEBUGL "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_THREAD "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer -fsanitize=thread -DKEGE_SANITIZE_THREAD")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
