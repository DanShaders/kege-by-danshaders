#include "stacktrace.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <unistd.h>

#include "logging.h"
#include "unwind-cxx.h"
using namespace stacktrace;

#if KEGE_EXCEPTION_STACKTRACE
#	include <backtrace.h>

namespace {
backtrace_state *state;
bool inited = false;

void init_error_cb(void *, const char *msg, int errnum) {
	logging::error("stacktrace init failed: " + std::to_string(errnum) + msg);
}

void stacktrace_error_cb(void *, const char *, int) {}

const exception_st empty_st{};

const int ST_INITIAL_CNT = 1;
const int ST_INITIAL_SIZE = sizeof(exception_st) + ST_INITIAL_CNT * sizeof(exception_st::st_entry);

int stacktrace_entry_cb(exception_st **pst, uintptr_t pc, const char *filename, int lineno,
						const char *function) {
	exception_st *&st = *pst;
	if (!st) {
		st = (exception_st *) malloc(ST_INITIAL_SIZE);
		if (!st) {
			return 1;
		}
		memset(st, 0, ST_INITIAL_SIZE);
		st->alloced_length = ST_INITIAL_CNT;
	}
	if (st->length == st->alloced_length) {
		exception_st *nst = (exception_st *) realloc(
			st, sizeof(exception_st) + (2 * st->alloced_length) * sizeof(exception_st::st_entry));
		if (!nst) {
			return 1;
		}
		st = nst;
		st->alloced_length *= 2;
	}

	int status = -1;
	char *buff = nullptr;
	if (function && function[0] == '_') {
		buff = __cxxabiv1::__cxa_demangle(function, 0, 0, &status);
	}
	st->entry[st->length++] = {pc, filename ? strdup(filename) : nullptr,
							   !status ? buff : (function ? strdup(function) : nullptr), lineno};
	return 0;
}

void free_exception_st(exception_st **pst) {
	auto st = *pst;
	if (!st) {
		return;
	}
	for (int i = 0; i < st->length; ++i) {
		if (st->entry[i].filename) {
			free(st->entry[i].filename);
		}
		if (st->entry[i].function) {
			free(st->entry[i].function);
		}
	}
	free(st);
}

void (*real__cxa_throw)(void *, std::type_info *, void (*)(void *)) = nullptr;
}  // namespace

namespace __cxxabiv1 {
struct __cxa_stacktraced_exception {
	exception_st *stacktrace;
	__cxa_refcounted_exception exc;
};

extern "C" __attribute__((nothrow)) void *__cxa_allocate_exception(std::size_t thrown_size) {
	thrown_size += sizeof(__cxa_stacktraced_exception);
	void *ret = malloc(thrown_size);

	if (!ret) {
		std::terminate();
	}

	memset(ret, 0, sizeof(__cxa_stacktraced_exception));
	return (void *) ((char *) ret + sizeof(__cxa_stacktraced_exception));
}

extern "C" __attribute__((nothrow)) void __cxa_free_exception(void *vptr) {
	char *ptr = (char *) vptr - sizeof(__cxa_stacktraced_exception);
	free_exception_st((exception_st **) ptr);
	free(ptr);
}

extern "C" __attribute__((nothrow)) void __cxa_throw(void *thrown_exception, std::type_info *tinfo,
													 void (*dest)(void *)) {
	if (!inited) [[unlikely]] {
		// Exception during static initialization, it is unlikely that we will ever want a
		// stacktrace of it.
		((decltype(real__cxa_throw)) dlsym(RTLD_NEXT, "__cxa_throw"))(thrown_exception, tinfo,
																	  dest);
	}

	exception_st *st = nullptr;
	backtrace_full(state, 1, (backtrace_full_callback) stacktrace_entry_cb, stacktrace_error_cb,
				   &st);
	*(exception_st **) ((__cxa_stacktraced_exception *) thrown_exception - 1) = st;
	real__cxa_throw(thrown_exception, tinfo, dest);
	__builtin_unreachable();
}
}  // namespace __cxxabiv1

void stacktrace::init() {
	real__cxa_throw = (decltype(real__cxa_throw)) dlsym(RTLD_NEXT, "__cxa_throw");
	state = backtrace_create_state(nullptr, 1, init_error_cb, 0);
	if (!state || !real__cxa_throw) {
		throw std::exception();
	}
	inited = true;
}

const exception_st *stacktrace::get_stacktrace(const std::exception_ptr &ptr) {
	auto st = (*(const __cxxabiv1::__cxa_stacktraced_exception **) (&ptr) - 1)->stacktrace;
	return st;
}

void stacktrace::async_update_stacktrace(const std::exception_ptr &ptr) {
	auto &st = (*(__cxxabiv1::__cxa_stacktraced_exception **) (&ptr) - 1)->stacktrace;
	if (st) {
		int len = st->length;
		backtrace_full(state, 2, (backtrace_full_callback) stacktrace_entry_cb, stacktrace_error_cb,
					   &st);
		if (st->length != len) {
			st->entry[len].is_async = true;
		}
	}
}

#else

void stacktrace::init() {}

const exception_st *stacktrace::get_stacktrace(const std::exception_ptr &) {
	return nullptr;
}

void stacktrace::async_update_stacktrace(const std::exception_ptr &) {}
#endif