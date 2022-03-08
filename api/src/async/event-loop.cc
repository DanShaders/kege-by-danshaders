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
	try {
		switch (payload.index()) {
			case 1: {
				event_loop::local->handle_exception(std::get<throw_exception>(payload).exc);
				break;
			}

			case 2: {
				const auto &c = std::get<resume_coroutine>(payload);
				if (!c.done_func(c.handle)) {
					c.resume_func(c.handle);
				}
				break;
			}

			case 3: {
				std::get<run_function>(payload).func();
				break;
			}

			case 4: {
				const auto &c = std::get<run_plain_function>(payload);
				c.func(c.arg);
				break;
			}

			case 0:
			default:
				break;
		}
	} catch (...) { event_loop::local->handle_exception(std::current_exception()); }
	payload = std::monostate{};
}

bool event_loop_work::has_work() const {
	return payload.index();
}

/* ==== async::event_loop ==== */
thread_local std::shared_ptr<event_loop> event_loop::local;

/* ==== async::detail ==== */
void detail::log_top_level_exception(const std::string &what) {
	int status = 0;
	char *buff = __cxxabiv1::__cxa_demangle(__cxxabiv1::__cxa_current_exception_type()->name(), 0,
											0, &status);
	std::cout << "Uncaught exception (in top-level async function): thrown "
				 "instance of '"
			  << buff << "'";
	if (what.size())
		std::cout << "\n  what():  " << what;
	std::cout << std::endl;
	std::free(buff);
}