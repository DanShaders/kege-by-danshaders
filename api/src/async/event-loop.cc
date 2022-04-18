#include "async/event-loop.h"
using namespace async;

#include <cxxabi.h>

#include "logging.h"

/* ==== async::completion_token ==== */
completion_token::completion_token() {}
completion_token::completion_token(std::shared_ptr<std::atomic_flag> flag_) : flag(flag_) {}

void completion_token::wait() {
	if (flag) {
		while (!flag->test()) {
			flag->wait(false);
		}
	}
}

/* ==== async::event_loop_work ==== */
event_loop_work::event_loop_work() : payload() {}

event_loop_work::event_loop_work(std::exception_ptr &&exc) {
	payload = throw_exception{exc};
}

event_loop_work::event_loop_work(std::function<void()> &&func) {
	payload = run_function{func};
}

event_loop_work::event_loop_work(void (*func)(void *), void *arg) {
	payload = run_plain_function{func, arg};
}

void event_loop_work::do_work() {
	auto current = std::move(payload);
	payload = std::monostate{};

	try {
		switch (current.index()) {
			case 1: {
				event_loop::local->handle_exception(std::get<throw_exception>(current).exc);
				break;
			}

			case 2: {
				const auto &c = std::get<resume_coroutine>(current);
				if (!c.done_func(c.handle)) {
					c.resume_func(c.handle);
				}
				break;
			}

			case 3: {
				std::get<run_function>(current).func();
				break;
			}

			case 4: {
				const auto &c = std::get<run_plain_function>(current);
				c.func(c.arg);
				break;
			}

			case 0:
			default:
				break;
		}
	} catch (...) { event_loop::local->handle_exception(std::current_exception()); }
}

bool event_loop_work::has_work() const {
	return payload.index();
}

/* ==== async::event_loop ==== */
thread_local std::shared_ptr<event_loop> event_loop::local;

/* ==== async::detail ==== */
void detail::log_top_level_exception(const std::string &what) {
	using namespace std::literals;

	int status = 0;
	char *buff = __cxxabiv1::__cxa_demangle(__cxxabiv1::__cxa_current_exception_type()->name(), 0,
											0, &status);
	logging::info("Uncaught exception (in top-level async function): thrown instance of '"s + buff +
				  "'");
	if (what.size()) {
		std::cout << "  what():  " << what << std::endl;
	}
	std::free(buff);
}