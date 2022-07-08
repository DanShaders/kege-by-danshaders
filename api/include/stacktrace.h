/**
 * Exception printing & stacktrace catching.
 * @see        stacktrace
 * @file
 */
#pragma once

#include "stdafx.h"

/**
 * Exception printing & stacktrace catching.
 *
 * The magic of capturing stacktrace is done via wrapping a few functions from C++ ABI. This is
 * definitely not the most portable thing on the Earth but at least it works under GCC on Linux.
 */
namespace stacktrace {
/**
 * Holds an exception stacktrace.
 *
 * @warning    Always preserve returned pointer to the structure and do not try to copy the object
 *             itself or assign it to a stack variable as it contains VLA. (At least run your code
 *             with sanitizers after such manipulations).
 */
struct exception_st {
	int length;         /**< Number of stacktrace entries */
	int alloced_length; /**< Number of stacktrace entries the memory was allocated for */

	/** Stacktrace entry */
	struct st_entry {
		uintptr_t pc;   /**< PC address */
		char *filename; /**< Deduced filename, may be nullptr */
		char *function; /**< Deduced function name, may be nullptr */
		int lineno;     /**< Deduced line number */
		bool is_async;  /**< If this is first entry appended by @ref async_update_stacktrace */
	} entry[0];
};

/**
 * Initializes stacktrace catching library.
 *
 * @see        #KEGE_EXCEPTION_STACKTRACE
 */
void init();

/**
 * Gets the stacktrace from the exception.
 *
 * @param[in]  ptr   Pointer to the exception.
 *
 * @return     stacktrace.
 */
const exception_st *get_stacktrace(const std::exception_ptr &ptr);

/**
 * Should be called to record stacktrace between asynchronous suspensions/continuations.
 *
 * @param[in]  ptr   Pointer to the exception.
 */
void async_update_stacktrace(const std::exception_ptr &ptr);

/**
 * Logs an unhandled exception using @ref logging::error.
 *
 * This function does it best to recover the correct stacktrace even in the presence of coroutines
 * via some heuristics.
 *
 * @param[in]  e     Pointer to the exception
 * @note       Defined in `src/stacktrace-specific.cc`.
 */
void log_unhandled_exception(const std::exception_ptr &e);

/**
 * Terminate handler to be set globally to intercept exceptions causing `std::terminate` to be
 * called.
 *
 * @note       Defined in `src/stacktrace-specific.cc`.
 */
void terminate_handler();
}  // namespace stacktrace