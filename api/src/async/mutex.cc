#include "async/mutex.h"
using namespace async;

/* ==== async::mutex::state_t ==== */
struct mutex::state_t {
	enum { UNLOCKED, LOCKED, DESTROYED } status = UNLOCKED;
	std::forward_list<event_loop_work> queue;
	decltype(queue)::iterator queue_end;
	std::optional<std::exception_ptr> exc;

	state_t() : queue_end(queue.before_begin()) {}

	void unlock() noexcept {
		if (status == DESTROYED) {
			return;
		}
		status = UNLOCKED;
		if (!queue.empty()) {
			auto work = std::move(queue.front());
			if (queue.begin() == queue_end) {
				queue_end = queue.before_begin();
			}
			queue.pop_front();
			work.do_work();
		}
	}
};

/* ==== async::mutex::locker ==== */
mutex::locker::locker(std::shared_ptr<state_t> state_) : state(state_) {}

void mutex::locker::await_suspend(event_loop_work &&work) {
	state->queue_end = state->queue.insert_after(state->queue_end, std::move(work));
}

bool mutex::locker::await_ready() noexcept {
	return state->status != state->LOCKED;
}

void mutex::locker::await_resume() {
	if (state->exc) {
		std::rethrow_exception(*state->exc);
	} else {
		state->status = state->LOCKED;
	}
}

/* ==== async::mutex ==== */
mutex::mutex() : state(std::make_shared<state_t>()) {}

mutex::locker mutex::lock() {
	if (state->status == state->DESTROYED) {
		throw std::runtime_error("trying to lock destroyed mutex");
	}
	return {state};
}

bool mutex::try_lock() {
	if (state->status == state->DESTROYED) {
		throw std::runtime_error("trying to lock destroyed mutex");
	} else if (state->status == state->UNLOCKED) {
		state->status = state->LOCKED;
		return true;
	} else {
		return false;
	}
}

void mutex::unlock() noexcept {
	state->unlock();
}

void mutex::cancel_all(const std::exception_ptr &exc) noexcept {
	state->status = state->DESTROYED;
	state->exc = exc;
	while (!state->queue.empty()) {
		auto work = std::move(state->queue.front());
		state->queue.pop_front();
		work.do_work();
	}
}

/* ==== async::lock_guard ==== */
lock_guard::lock_guard(const mutex &m) : locker(m.state) {}

lock_guard::~lock_guard() {
	state->unlock();
}