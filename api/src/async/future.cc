#include "async/future.h"
using namespace async;

/* ==== async::detail::future_base ==== */
void detail::future_base::set_ready() noexcept {
	assert(status != FULFILLED && status != READY);
	if (status == AWAITING) {
		status = FULFILLED;
		work.do_work();
	} else {
		status = READY;
	}
}

bool detail::future_base::await_ready() const noexcept {
	return status == READY;
}

void detail::future_base::await_suspend(std::coroutine_handle<> h) noexcept {
	status = AWAITING;
	work = event_loop_work(h);
}

void detail::future_base::clear() noexcept {
	assert(status != AWAITING);
	status = CLEAR;
}

bool detail::future_base::is_awaiting() const noexcept {
	return status == AWAITING;
}

/* ==== async::future<void> ==== */
void future<void>::await_resume() {
	status = FULFILLED;
	if (result) {
		std::rethrow_exception(*result);
	}
}

void future<void>::set_exception(const std::exception_ptr &exc) noexcept {
	result = exc;
	set_ready();
}

void future<void>::set_result() noexcept {
	set_ready();
}