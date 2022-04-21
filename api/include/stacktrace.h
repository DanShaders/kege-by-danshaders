#pragma once

#include "stdafx.h"

namespace stacktrace {
struct exception_st {
	int length, alloced_length;
	struct st_entry {
		uintptr_t pc;
		char *filename, *function;
		int lineno;
		bool is_async;
	} entry[0];
};

void init();
const exception_st *get_stacktrace(const std::exception_ptr &ptr);
void async_update_stacktrace(const std::exception_ptr &ptr);

// src/stacktrace-specific.cc
void log_unhandled_exception(const std::exception_ptr &e);

// src/stacktrace-specific.cc
void terminate_handler();
}  // namespace stacktrace