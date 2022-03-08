#include "async/libev-event-loop.h"
using namespace async;

#include <ev.h>
#include <fcgiapp.h>
#include <sys/errno.h>
#include <unistd.h>

#include "logging.h"

using detail::ev_with_arg;

/* ==== async::sleep ==== */
sleep::sleep(std::chrono::nanoseconds duration_) {
	duration = double(duration_.count()) / 1e9;
}

bool sleep::await_ready() {
	return duration <= 0;
}

void sleep::await_resume() {}

/* ==== async::libev_event_loop::impl ==== */
struct data_t {
	enum { NEW_TIMEOUT, NEW_SOCKET, MOD_SOCKET, DEL_SOCKET, CLOSE_FCGX, CLOSE_SOCKET } type;

	union {
		double vdouble;
		int vint;
		void *vptr;
	} ext;
};

typedef struct ev_loop ev_loop_t;

struct libev_event_loop::impl {
	std::vector<std::shared_ptr<event_source>> sources;
	ev_loop_t *loop;

	ev_with_arg<ev_async> stop_notifier;

	ev_with_arg<ev_async> work_notifier;
	std::shared_ptr<std::atomic_flag> ts_work_token;
	event_loop_work ts_work;

	ev_with_arg<ev_prepare> worker;

	std::queue<event_loop_work> queue;
	bool until_complete = 1;
	bool success_flag = 1;

	static void timer_cb(ev_loop_t *loop, ev_timer *w_, int) {
		auto w = (ev_with_arg<ev_timer, data_t> *) (w_);
		ev_timer_stop(loop, &w->w);
		((sleep *) w->arg.ext.vptr)->work.do_work();
		delete w;
	}

	static void socket_cb(ev_loop_t *, ev_io *w, int) {
		auto storage = (socket_storage *) ((ev_with_arg<ev_io> *) w)->arg;
		if (storage->sock_func(storage->sock_this, true)) {
			storage->work.do_work();
		}
	}

	void process_event(data_t data) {
		switch (data.type) {
			case data_t::NEW_TIMEOUT: {
				auto *timer_event = new ev_with_arg<ev_timer, data_t>{{}, std::move(data)};
				ev_timer_init(&timer_event->w, timer_cb,
							  ((sleep *) timer_event->arg.ext.vptr)->duration, 0.);
				ev_timer_start(loop, &timer_event->w);
				break;
			}

			case data_t::NEW_SOCKET: {
				auto storage = (socket_storage *) data.ext.vptr;
				auto io_event = new ev_with_arg<ev_io>{{}, storage};
				storage->event = io_event;
				ev_io_init(&io_event->w, socket_cb, storage->fd, storage->flags);
				ev_io_start(loop, &io_event->w);
				break;
			}
			case data_t::MOD_SOCKET: {
				auto storage = (socket_storage *) data.ext.vptr;
				auto io_event = (ev_with_arg<ev_io> *) storage->event;
				ev_io_stop(loop, &io_event->w);
				ev_io_init(&io_event->w, socket_cb, storage->fd, storage->flags);
				ev_io_start(loop, &io_event->w);
				break;
			}

			case data_t::DEL_SOCKET: {
				auto storage = (socket_storage *) data.ext.vptr;
				auto event = (ev_with_arg<ev_io> *) storage->event;
				ev_io_stop(loop, &event->w);
				delete event;
				delete storage;
				break;
			}

			case data_t::CLOSE_FCGX: {
				FCGX_Finish_r((FCGX_Request *) data.ext.vptr);
				delete (FCGX_Request *) data.ext.vptr;
				break;
			}

			case data_t::CLOSE_SOCKET: {
				close(data.ext.vint);
				break;
			}
		}
	}

	bool run(bool until_complete_) {
		for (auto source : sources)
			source->on_init();
		until_complete = until_complete_;
		if (!until_complete || event_loop::local->alive_coroutines) {
			ev_run(loop, 0);
		}

		bool ret = success_flag;
		success_flag = true;
		return ret;
	}

	static void stop_notifier_cb(ev_loop_t *, ev_async *w, int) {
		auto p = (impl *) ((ev_with_arg<ev_async> *) w)->arg;

		p->until_complete = true;
	}

	static void work_notifier_cb(ev_loop_t *, ev_async *w, int) {
		auto p = (impl *) ((ev_with_arg<ev_async> *) w)->arg;

		p->ts_work.do_work();
		p->ts_work_token->test_and_set();
		p->ts_work_token->notify_all();
	}

	static void worker_cb(ev_loop_t *loop, ev_prepare *w, int) {
		auto p = (impl *) ((ev_with_arg<ev_prepare> *) w)->arg;

		while (p->queue.size()) {
			p->queue.front().do_work();
			p->queue.pop();
		}

		if (p->queue.empty() && p->until_complete && !event_loop::local->alive_coroutines) {
			ev_break(loop, EVBREAK_ALL);
		}
	}

	void stop_notify() {
		ev_async_send(loop, &stop_notifier.w);
	}

	completion_token schedule_work_ts(event_loop_work &&work) {
		completion_token(ts_work_token).wait();
		ts_work_token = std::make_shared<std::atomic_flag>();
		ts_work = std::move(work);
		ev_async_send(loop, &work_notifier.w);
		return {ts_work_token};
	}

	impl() {
		loop = ev_loop_new(0);
		if (!loop)
			throw std::runtime_error("unable to create libev event loop");

		ev_async_init(&stop_notifier.w, stop_notifier_cb);
		stop_notifier.arg = this;
		ev_async_start(loop, &stop_notifier.w);

		ev_async_init(&work_notifier.w, work_notifier_cb);
		work_notifier.arg = this;
		ev_async_start(loop, &work_notifier.w);

		ev_prepare_init(&worker.w, worker_cb);
		worker.arg = this;
		ev_prepare_start(loop, &worker.w);
	}

	~impl() {
		for (auto source : sources)
			source->on_stop();
		ev_async_stop(loop, &stop_notifier.w);
		ev_async_stop(loop, &work_notifier.w);
		ev_loop_destroy(loop);
	}
};

/* ==== async::libev_event_loop ==== */
std::shared_ptr<libev_event_loop> libev_event_loop::get() {
	return std::dynamic_pointer_cast<libev_event_loop, event_loop>(local);
}

libev_event_loop::libev_event_loop() : pimpl(new impl{}) {}

libev_event_loop::~libev_event_loop() {
	delete pimpl;
}

void *libev_event_loop::get_underlying_loop() {
	return pimpl->loop;
}

bool libev_event_loop::run(bool until_complete) {
	if (!pimpl) {
		throw std::invalid_argument("unable to start event loop again");
	}
	return pimpl->run(until_complete);
}

void libev_event_loop::stop_notify() {
	pimpl->stop_notify();
}

void libev_event_loop::schedule_work(event_loop_work &&work) {
	pimpl->queue.push(std::move(work));
}

void libev_event_loop::handle_exception(const std::exception_ptr &exc) {
	try {
		std::rethrow_exception(exc);
	} catch (const std::exception &e) { detail::log_top_level_exception(e.what()); } catch (...) {
		detail::log_top_level_exception();
	}
	pimpl->success_flag = false;
}

completion_token libev_event_loop::schedule_work_ts(event_loop_work &&work) {
	return pimpl->schedule_work_ts(std::move(work));
}

void libev_event_loop::register_source(std::shared_ptr<event_source> source) {
	pimpl->sources.push_back(source);
}

void libev_event_loop::bind_to_thread() {
	for (auto source : pimpl->sources)
		source->bind_to_thread();
	local = shared_from_this();
}

void libev_event_loop::schedule_timer_resume(sleep *obj) {
	pimpl->process_event({data_t::NEW_TIMEOUT, {.vptr = obj}});
}

void libev_event_loop::schedule_fcgx_close(void *req) {
	pimpl->process_event({.type = data_t::CLOSE_FCGX, .ext = {.vptr = req}});
}

void libev_event_loop::schedule_socket_close(int fd) {
	pimpl->process_event({.type = data_t::CLOSE_SOCKET, .ext = {.vint = fd}});
}

void libev_event_loop::socket_add(socket_storage *storage) {
	pimpl->process_event({.type = data_t::NEW_SOCKET, .ext = {.vptr = storage}});
}

void libev_event_loop::socket_mod(socket_storage *storage) {
	pimpl->process_event({.type = data_t::MOD_SOCKET, .ext = {.vptr = storage}});
}

void libev_event_loop::socket_del(socket_storage *storage) {
	pimpl->process_event({.type = data_t::DEL_SOCKET, .ext = {.vptr = storage}});
}
