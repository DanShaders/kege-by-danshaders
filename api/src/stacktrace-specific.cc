#include <cxxabi.h>
#include <execinfo.h>

#include "KEGE.h"
#include "logging.h"
#include "stacktrace.h"
using namespace stacktrace;

void print_exception_to(const std::string &what, std::ostream &out, bool is_nested = false) {
	int status = 0;
	const char *exc_type = __cxxabiv1::__cxa_current_exception_type()->name();
	char *buff = __cxxabiv1::__cxa_demangle(exc_type, 0, 0, &status);

	if (!is_nested) {
		out << "Exception in thread \"" << logging::get_thread_name() << "\" ";
	} else {
		out << "Caused by: ";
	}

	if (status) {
		out << exc_type;
	} else {
		auto len = strlen(buff);
		if (!strncmp(buff, "std::_Nested_exception<", 23) && buff[len - 1] == '>') {
			out << std::string_view(buff + 23, len - 24);
		} else {
			out << buff;
		}
		std::free(buff);
	}

	if (what.size() &&
		(what != "std::exception" || (strcmp(exc_type, "St9exception") &&
									  strcmp(exc_type, "St17_Nested_exceptionISt9exceptionE")))) {
		out << ": " << what;
	}
	out << "\n";

#if KEGE_EXCEPTION_STACKTRACE
	auto st = stacktrace::get_stacktrace(std::current_exception());
	if (!st) {
		return;
	}

	bool in_api_handler = false;
	for (auto s : std::span(st->entry, st->length)) {
		if (s.filename) {
			std::string_view file = s.filename;
			if (file.starts_with(SOURCE_DIR "routes/") && file != SOURCE_DIR "routes/wrap.cc") {
				in_api_handler = true;
				break;
			}
		}
	}

	for (int lef = 0; lef < st->length;) {
		int rig = lef + 1;
		for (; rig < st->length && !st->entry[rig].is_async; ++rig)
			;

		bool should_print_awaiting = !!lef;
		for (auto s : std::span(st->entry + lef, rig - lef)) {
			if (!s.filename || !s.lineno || !s.function) {
				if (s.pc == ~0ull) {
					continue;
				}
				char **symbol = backtrace_symbols((void **) &s.pc, 1);
				out << "  at " << (should_print_awaiting ? "[awaiting] " : "") << symbol[0] << "\n";
				should_print_awaiting = false;
				std::free(symbol);
			} else {
				std::string_view file = s.filename, function = s.function;
				if (file.starts_with(SOURCE_DIR)) {
					file = file.substr(strlen(SOURCE_DIR));
					if (in_api_handler &&
						(file == "routes/_wrap.cc" || file == "src/async/event-loop.cc")) {
						break;
					}
				} else {
					if (function.starts_with("void std::__throw_with_nested_impl<") ||
						function.starts_with("void std::throw_with_nested<")) {
						continue;
					}
					if (function.starts_with("std::__n4861::coroutine_handle<") &&
						function.ends_with(">::resume() const")) {
						continue;
					}
				}
				out << "  at " << (should_print_awaiting ? "[awaiting] " : "") << function << " ("
					<< file << ":" << s.lineno << ")\n";
				should_print_awaiting = false;
			}
		}
		lef = rig;
	}
#endif
}

template <typename T>
static void log_exception_chain(const T &e, std::ostream &out, bool is_nested = false) {
	try {
		if constexpr (std::same_as<T, std::exception_ptr>) {
			std::rethrow_exception(e);
		} else {
			std::rethrow_if_nested(e);
		}
	} catch (const std::exception &next) {
		print_exception_to(next.what(), out, is_nested);
		log_exception_chain(next, out, true);
	} catch (...) { print_exception_to("", out, true); }
}

void stacktrace::log_unhandled_exception(const std::exception_ptr &e) {
	std::ostringstream oss;
	log_exception_chain(e, oss);
	logging::warn(oss.str());
}

inline std::atomic_bool should_exit;

void stacktrace::terminate_handler() {
	if (should_exit) {
		return;
	}
	should_exit = true;

	if (!std::current_exception()) {
		logging::error("terminate called without an active exception");
	} else {
		std::ostringstream oss;
		log_exception_chain(std::current_exception(), oss);
		logging::error(oss.str());
	}
}
