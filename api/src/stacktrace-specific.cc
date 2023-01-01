#include <cxxabi.h>
#include <execinfo.h>

#include "KEGE.h"
#include "stacktrace.h"
using namespace stacktrace;

static void print_exception(std::string const& what, fmtlog::LogLevel log_level,
                            bool is_nested = false) {
  char const* exc_type = __cxxabiv1::__cxa_current_exception_type()->name();

  std::string_view exc_typename{exc_type};
  int status = 0;
  char* buff = __cxxabiv1::__cxa_demangle(exc_type, 0, 0, &status);
  if (!status) {
    exc_typename = buff;
    if (exc_typename.starts_with("std::_Nested_exception<") && exc_typename.ends_with(">")) {
      exc_typename = exc_typename.substr(23, exc_typename.size() - 24);
    }
  }

  FMTLOG(log_level, "{} {}: {}", !is_nested ? "Exception" : "Caused by", exc_typename, what);

  if (!status) {
    std::free(buff);
  }

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
        char** symbol = backtrace_symbols((void**) &s.pc, 1);
        FMTLOG(log_level, "  at {}{}", should_print_awaiting ? "[awaiting] " : "", symbol[0]);
        should_print_awaiting = false;
        std::free(symbol);
      } else {
        std::string_view file = s.filename, function = s.function;
        if (file.starts_with(SOURCE_DIR)) {
          file = file.substr(strlen(SOURCE_DIR));
          if (in_api_handler && (file == "routes/_wrap.cc" || file == "src/async/event-loop.cc")) {
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
        FMTLOG(log_level, "  at {}{} ({}:{})", should_print_awaiting ? "[awaiting] " : "", function,
               file, s.lineno);
        should_print_awaiting = false;
      }
    }
    lef = rig;
  }
#endif
}

template <typename T>
static void log_exception_chain(T const& e, fmtlog::LogLevel log_level, bool is_nested = false) {
  try {
    if constexpr (std::same_as<T, std::exception_ptr>) {
      std::rethrow_exception(e);
    } else {
      std::rethrow_if_nested(e);
    }
  } catch (std::exception const& next) {
    print_exception(next.what(), log_level, is_nested);
    log_exception_chain(next, log_level, true);
  } catch (...) {
    print_exception("", log_level, true);
  }
}

void stacktrace::log_unhandled_exception(std::exception_ptr const& e) {
  log_exception_chain(e, fmtlog::WRN);
}

inline std::atomic_bool should_exit;

void stacktrace::terminate_handler() {
  if (should_exit) {
    return;
  }
  should_exit = true;

  if (!std::current_exception()) {
    loge("terminate called without an active exception");
  } else {
    log_exception_chain(std::current_exception(), fmtlog::ERR);
  }
  fmtlog::poll();
}
