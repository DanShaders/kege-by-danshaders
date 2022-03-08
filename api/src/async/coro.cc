#include "async/coro.h"
using namespace async;

/* ==== async::detail::promise_type_facade ==== */
std::suspend_never detail::promise_type_facade::initial_suspend() noexcept {
	return {};
}

void detail::promise_type_facade::final_suspend_inner(void *handle, void (*func)(void *)) noexcept {
	if (is_top_level) {
		--event_loop::local->alive_coroutines;
		if (exc.has_value()) {
			event_loop::local->handle_exception(exc.value());
		}
		if (destroy_on_done) {
			event_loop::local->schedule_work(event_loop_work(func, handle));
		}
	} else if (parent.has_value()) {
		event_loop::local->schedule_work(std::move(parent.value()));
	}
}

void detail::promise_type_facade::unhandled_exception() {
	exc = std::current_exception();
}

/* ==== async::suspend_t ==== */
bool suspend_t::await_ready() noexcept {
	return false;
}

void suspend_t::await_resume() noexcept {}

/* ==== async::detail ==== */
bool detail::run_until_complete(int alive) {
	event_loop::local->alive_coroutines = alive;
	return event_loop::local->run(true);
}
