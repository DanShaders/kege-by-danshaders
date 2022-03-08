#pragma once

#include "stdafx.h"

#include "event-loop.h"

namespace async {
class mutex;
class lock_guard;

class mutex {
	DEFAULT_MOVABLE_CLASS(mutex)
	DEFAULT_COPIABLE_CLASS(mutex)

	friend lock_guard;

private:
	struct state_t;

	std::shared_ptr<state_t> state;

public:
	class locker {
		IMMOVABLE_CLASS(locker)
		UNCOPIABLE_CLASS(locker)

		friend mutex;

	protected:
		std::shared_ptr<state_t> state;

		locker(std::shared_ptr<state_t> state_);
		void await_suspend(event_loop_work &&work);

	public:
		bool await_ready() noexcept;
		void await_resume();
		template <typename T>
		void await_suspend(std::coroutine_handle<T> &h) noexcept;
	};

	mutex();

	locker lock();
	bool try_lock();
	void unlock() noexcept;
	void cancel_all(const std::exception_ptr &exc) noexcept;
};

class lock_guard : public mutex::locker {
public:
	lock_guard(const mutex &m);
	~lock_guard();
};

#include "detail/mutex.impl.h"
}  // namespace async