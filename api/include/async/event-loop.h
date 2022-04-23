#pragma once

#include "stdafx.h"

namespace async {
class completion_token;
class event_loop_work;
class event_loop;
class event_source;

class completion_token {
private:
	std::shared_ptr<std::atomic_flag> flag;

public:
	completion_token();
	completion_token(std::shared_ptr<std::atomic_flag> flag_);

	void wait();
};

class event_loop_work {
private:
	struct resume_coroutine {
		void *handle;
		bool (*done_func)(void *);
		void (*resume_func)(void *);
	};

	struct throw_exception {
		std::exception_ptr exc;
	};

	struct run_function {
		std::function<void()> func;
	};

	struct run_plain_function {
		void (*func)(void *);
		void *arg;
	};

	std::variant<std::monostate, throw_exception, resume_coroutine, run_function,
				 run_plain_function>
		payload;

public:
	event_loop_work();
	template <typename T>
	event_loop_work(std::coroutine_handle<T> *handle);
	event_loop_work(std::exception_ptr &&exc);
	event_loop_work(std::function<void()> &&func);
	event_loop_work(void (*func)(void *), void *arg);
	event_loop_work(event_loop_work &) = delete;
	event_loop_work(event_loop_work &&) = default;

	bool has_work() const;
	void do_work();

	event_loop_work &operator=(event_loop_work &) = delete;
	event_loop_work &operator=(event_loop_work &&) = default;
};

enum socket_event_type : char { SOCK_NONE = 0, READABLE = 1, WRITABLE = 2, SOCK_ALL = 3 };

struct socket_storage {
	event_loop_work work;
	void *event = nullptr;
	int fd = -1;
	socket_event_type event_mask = SOCK_NONE, last_event = SOCK_NONE;
};

class event_loop : public std::enable_shared_from_this<event_loop> {
public:
	static thread_local std::shared_ptr<event_loop> local;

	int alive_coroutines = 0;

	virtual ~event_loop() {}

	virtual bool run(bool until_complete) = 0;
	virtual void stop_notify() = 0;
	virtual void schedule_work(event_loop_work &&work) = 0;
	virtual void handle_exception(const std::exception_ptr &exc) = 0;

	virtual completion_token schedule_work_ts(event_loop_work &&work) = 0;

	virtual void register_source(std::shared_ptr<event_source> source) = 0;

	virtual void bind_to_thread() = 0;
};

class event_source : public std::enable_shared_from_this<event_source> {
public:
	virtual ~event_source() {}

	virtual void bind_to_thread() {}
	virtual void on_init() {}
	virtual void on_stop() {}
};

namespace detail {
	inline std::function<void(const std::exception_ptr &)> on_unhandled_exception_cb;

	void log_top_level_exception(const std::exception_ptr &e);
}  // namespace detail

#include "detail/event-loop.impl.h"
}  // namespace async
