#pragma once

// Whether or not to print debug log messages.
#define KEGE_LOG_DEBUG_ENABLED 1

// If this is on, the FastCGI server will read client request bodies asynchronously. This is usually
// not needed because of proxy server request buffering; for example, nginx turns it on by default.
// Note: async implementation is considerably slower (cpu time, ~1.5 times) than the sync one.
#define KEGE_FCGI_USE_ASYNC 0

inline const int FCGI_QUEUE_SIZE = 1000;

// clang-format off
#define KEGE_VERSION_MAJOR @KEGE_VERSION_MAJOR@
#define KEGE_VERSION_MINOR @KEGE_VERSION_MINOR@
#define KEGE_VERSION_PATCH @KEGE_VERSION_PATCH@
#define KEGE_VERSION           \
	"@KEGE_VERSION_MAJOR@" "." \
	"@KEGE_VERSION_MINOR@" "." \
	"@KEGE_VERSION_PATCH@"
#define BUILD_TYPE "@CMAKE_BUILD_TYPE@"
// clang-format on