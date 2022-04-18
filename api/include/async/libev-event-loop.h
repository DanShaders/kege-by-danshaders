#pragma once

#include "stdafx.h"

#include "event-loop.h"

namespace async {
class sleep;
struct socket_storage;
class libev_event_loop;

class sleep {
private:
	friend class libev_event_loop;

	double duration;
	event_loop_work work;

public:
	sleep(std::chrono::nanoseconds duration_);

	bool await_ready();
	void await_resume();
	template <typename T>
	void await_suspend(std::coroutine_handle<T> &h);
};

struct socket_storage {
	event_loop_work read_work, write_work;
	void *event = nullptr;
	int fd = -1;
};

class libev_event_loop : public event_loop {
private:
	struct impl;
	impl *pimpl;

public:
	static std::shared_ptr<libev_event_loop> get();

	libev_event_loop();
	~libev_event_loop();

	void *get_underlying_loop();

	bool run(bool until_complete) override;
	void stop_notify() override;
	void schedule_work(event_loop_work &&work) override;
	void handle_exception(const std::exception_ptr &exc) override;

	completion_token schedule_work_ts(event_loop_work &&work) override;

	void register_source(std::shared_ptr<event_source> source) override;

	void bind_to_thread() override;

	void schedule_timer_resume(sleep *obj);

	void socket_add(socket_storage *storage);
	void socket_del(socket_storage *storage);
};

namespace detail {
	template <typename T, typename U = void *>
	struct ev_with_arg {
		T w;
		U arg;
	};
}  // namespace detail

#include "detail/libev-event-loop.impl.h"
}  // namespace async
