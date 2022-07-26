#include "async/event-loop.h"
using namespace async;

/* ==== async::event_loop_work ==== */
event_loop_work::event_loop_work() {}

event_loop_work::event_loop_work(std::coroutine_handle<> h) {
	handle = h;
	type = RESUME;
}

event_loop_work event_loop_work::create_destroyer(std::coroutine_handle<> h) {
	event_loop_work work;
	work.handle = h;
	work.type = DESTROY;
	return work;
}

void event_loop_work::do_work() {
	auto saved_type = type;
	type = NONE;

	try {
		if (saved_type == RESUME) {
			if (!handle.done()) {
				handle.resume();
			}
		} else if (saved_type == DESTROY) {
			handle.destroy();
		}
	} catch (...) {
		event_loop::local->handle_exception(std::current_exception());
	}
}

bool event_loop_work::has_work() const {
	return type != NONE;
}

/* ==== async::event_loop ==== */
thread_local std::shared_ptr<event_loop> event_loop::local;

/* ==== async::detail ==== */
void detail::log_top_level_exception(const std::exception_ptr &e) {
	on_unhandled_exception_cb(e);
}
