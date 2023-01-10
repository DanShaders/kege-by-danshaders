/**
 * Configuration
 * @file
 */

// clang-format off
#pragma once

/** Whether or not to print debug log messages. */
#define KEGE_LOG_DEBUG_ENABLED @KEGE_LOG_DEBUG_ENABLED@

/** Whether or not to collect exception stacktraces. */
#define KEGE_EXCEPTION_STACKTRACE @KEGE_EXCEPTION_STACKTRACE@

/**
 * If this is on, the FastCGI server will read client request bodies asynchronously.
 *
 * @note       This is not needed because of proxy server request buffering; for example, nginx
 *             turns it on by default. Async implementation is considerably slower (cpu time, ~1.5
 *             times) than the sync one.
 */
#define KEGE_FCGI_USE_ASYNC 0

/** Whether the database is using floating-point numbers to store timestamps. */
#define KEGE_PQ_FP_TIMESTAMP 0

/** FCGI server socket queue size */
inline const int FCGI_QUEUE_SIZE = 1000;

/** @cond FALSE */
#define KEGE_VERSION_MAJOR @KEGE_VERSION_MAJOR@
#define KEGE_VERSION_MINOR @KEGE_VERSION_MINOR@
#define KEGE_VERSION_PATCH @KEGE_VERSION_PATCH@
#define KEGE_VERSION           \
	"@KEGE_VERSION_MAJOR@" "." \
	"@KEGE_VERSION_MINOR@" "." \
	"@KEGE_VERSION_PATCH@"
#define BUILD_TYPE "@CMAKE_BUILD_TYPE@"
#define SOURCE_DIR "@CMAKE_SOURCE_DIR@/"
/** @endcond */
// clang-format on
