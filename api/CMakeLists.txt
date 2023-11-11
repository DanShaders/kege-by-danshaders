if (CMAKE_BUILD_TYPE STREQUAL Release)
	set(KEGE_EXCEPTION_STACKTRACE 0)
	set(KEGE_LOG_DEBUG_ENABLED 0)
else()
	set(KEGE_EXCEPTION_STACKTRACE 1)
	set(KEGE_LOG_DEBUG_ENABLED 1)
endif()
configure_file(include/KEGE.in.h KEGE.h)

set(
	DB_CONNECT_STR
	"host=${CMAKE_BINARY_DIR}/../run user=kege dbname=kege"
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
	routes/groups.cc
	routes/jobs.cc
	routes/kims.cc
	routes/standings.cc
	routes/task-types.cc
	routes/tasks.cc
	routes/user.cc
	routes/users.cc
)

set(
	KEGE_SQL_SOURCES
	routes/contestant.sql
	routes/groups.sql
	routes/kims.sql
	routes/standings.sql
	routes/users.sql
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
	../proto/groups.proto
	../proto/jobs.proto
	../proto/kims.proto
	../proto/standings.proto
	../proto/task-types.proto
	../proto/tasks.proto
	../proto/user.proto
	../proto/users.proto
)


# ==== Dealing with SQL sources ====
foreach(filename IN LISTS KEGE_SQL_SOURCES)
	list(APPEND KEGE_SOURCES
		${filename}.cc
	)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${filename}.cc
		COMMAND $<TARGET_FILE:sql-typer> ${DB_CONNECT_STR} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
		DEPENDS
			${filename}
			../meta/postgres/db.sql
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
		rm -rf <SOURCE_DIR> &&
		git clone https://github.com/MengRao/fmtlog.git <SOURCE_DIR> &&
		cd <SOURCE_DIR> &&
		git checkout 01b6cd339101f36c37e50938ec11918c6229b2de &&
		git submodule update --init ./fmt &&
		echo > ./test/CMakeLists.txt &&
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


# ==== Target libev ====
ExternalProject_Add(
	EV
	GIT_REPOSITORY "https://github.com/xorangekiller/libev-git"
	GIT_TAG "c7d6986ec119831cebe0e815f83e7ba2fbdb1ae0"
	CMAKE_ARGS
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		-DCMAKE_BUILD_TYPE=Release
	UPDATE_COMMAND ""
	EXCLUDE_FROM_ALL
	BUILD_BYPRODUCTS
		<INSTALL_DIR>/lib/libev.a
)
ExternalProject_Get_property(EV INSTALL_DIR)
set(EV_INCLUDE_DIRS "${INSTALL_DIR}/include")
set(EV_LIBRARIES "${INSTALL_DIR}/lib/libev.a")


# ==== nlohmann_json ====
ExternalProject_Add(
	NLOHMANN_JSON
	DOWNLOAD_COMMAND
		rm -rf <SOURCE_DIR> && mkdir -p <SOURCE_DIR> && cd <SOURCE_DIR> &&
		git init && git remote add origin https://github.com/nlohmann/json.git &&
		git fetch origin bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d --depth=1 &&
		git reset --hard FETCH_HEAD &&
		cd -
	CMAKE_ARGS
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		-DCMAKE_BUILD_TYPE=Release
		-DJSON_BuildTests=OFF
	UPDATE_COMMAND ""
	EXCLUDE_FROM_ALL
)
ExternalProject_Get_property(NLOHMANN_JSON INSTALL_DIR)
set(NLOHMANN_JSON_INCLUDE_DIRS "${INSTALL_DIR}/include")


# ==== *.proto -> *.pb.cc ====
set(proto_cxx_all)
protobuf_generate(
	APPEND_PATH OUT_VAR proto_cxx_all
	LANGUAGE cpp_initializers
	GENERATE_EXTENSIONS .pb.h .pb.cc
	PLUGIN protoc-gen-cpp_initializers=$<TARGET_FILE:cpp-protoc-initializers>
	PROTOS ${PROTO_SOURCES}
)
foreach(_file ${proto_cxx_all})
	if(_file MATCHES "cc$")
		list(APPEND PROTO_CXX_SRC ${_file})
	endif()
endforeach()


# ==== Target KEGE_BASE ====
add_library(KEGE_BASE INTERFACE)

add_dependencies(KEGE_BASE LIBBACKTRACE EV LEXBOR FMTLOG NLOHMANN_JSON) 
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(FCGI REQUIRED IMPORTED_TARGET fcgi)
pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)
pkg_check_modules(PQ REQUIRED IMPORTED_TARGET libpq)


target_include_directories(
	KEGE_BASE INTERFACE

	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_BINARY_DIR}
	${BACKTRACE_INCLUDE_DIRS}
	${EV_INCLUDE_DIRS}
	${FMTLOG_INCLUDE_DIRS}
	${LEXBOR_INCLUDE_DIRS}
	${NLOHMANN_JSON_INCLUDE_DIRS}
	${URIPARSER_INCLUDE_DIRS}
)

target_link_libraries(
	KEGE_BASE INTERFACE
	${BACKTRACE_LIBRARIES}
	${EV_LIBRARIES}
	${FMTLOG_LIBRARIES}
	${LEXBOR_LIBRARIES}
	${NLOHMANN_JSON_LIBRARIES}
	${URIPARSER_LIBRARIES}
	pthread
	dl
	PkgConfig::CURL
	PkgConfig::FCGI
	PkgConfig::FMT
	PkgConfig::OPENSSL
	PkgConfig::PQ
	protobuf::libprotobuf
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


# ==== Additional compiler options ====
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize=alignment -DKEGE_SANITIZE_ADDRESS")
set(CMAKE_CXX_FLAGS_DEBUGL "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_THREAD "${CMAKE_CXX_FLAGS} -Og -g -fno-omit-frame-pointer -fsanitize=thread -DKEGE_SANITIZE_THREAD")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
