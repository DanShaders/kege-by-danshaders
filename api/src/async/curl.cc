#include "async/curl.h"
using namespace async;

#include <ev.h>

#include "async/libev-event-loop.h"
#include "logging.h"

using detail::ev_with_arg;

typedef struct ev_loop ev_loop_t;

/* ==== async::curl_storage ==== */
enum { INITIAL = 0, ENQUEUED, HEADERS_RECIEVED, COMPLETED, AFTER_DONE };

struct async::curl_storage {
	CURL *handle;
	event_loop_work work;
	int fire_at = AFTER_DONE;
	int state = INITIAL;
	int status = -1;

	std::vector<std::string> raw_headers;
	std::streambuf *body_buf;
	std::string body;
	std::string mime_type;

	void on_state_update(int new_state, int new_status);
	void write_body(char *ptr, std::size_t len);
};

void curl_storage::on_state_update(int new_state, int new_status) {
	if (new_state == COMPLETED && new_status != CURLE_OK) {
		char const *effective_url;
		curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &effective_url);
		if (effective_url == nullptr) {
			effective_url = "(nil)";
		}
		logging::warn(("curl failed with errno=" + std::to_string(new_status) +
					   " while requesting " + effective_url)
						  .c_str());
		status = 404;
	} else if (state < HEADERS_RECIEVED && new_state >= HEADERS_RECIEVED) {
		long response_code = 0;
		curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
		status = (int) response_code;

		char *pmime_type = 0;
		curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &pmime_type);
		if (pmime_type) {
			mime_type = pmime_type;
		}
	}
	state = new_state;
	if (fire_at <= state) {
		fire_at = AFTER_DONE;
		event_loop::local->schedule_work(std::move(work));
	}
}

void curl_storage::write_body(char *ptr, std::size_t len) {
	if (body_buf) {
		body_buf->sputn(ptr, len);
	} else {
		body.append(ptr, ptr + len);
	}
}

/* ==== async::curl ==== */
static CURL *get_curl_handle() {
	CURL *handle = curl_easy_init();
	if (handle) {
		curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 50L);
		curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
		curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
		curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
	} else {
		throw std::runtime_error("unable to initialize curl instance");
	}
	return handle;
}

curl::curl() {
	auto handle = get_curl_handle();
	storage = std::shared_ptr<curl_storage>(new curl_storage{}, [](auto s) {
		curl_easy_cleanup(s->handle);
		delete s;
	});
	storage->handle = handle;
}

void curl::reset() {
	curl_easy_reset(storage->handle);
}

CURLcode curl::set_url(const std::string &url) {
	return set_opt(CURLOPT_URL, url.c_str());
}

CURL *curl::get_handle() const {
	return storage->handle;
}

void curl::write_body_to(std::streambuf *buf) {
	storage->body_buf = buf;
}

curl_performer curl::get_headers() {
	storage->fire_at = HEADERS_RECIEVED;
	return curl_performer{storage};
}

curl_performer curl::perform() {
	storage->fire_at = COMPLETED;
	return curl_performer{storage};
}

/* ==== async::curl_result ==== */
curl_result::curl_result(std::shared_ptr<curl_storage> storage_) : storage(storage_) {}

int curl_result::status() const {
	return storage->status;
}

const std::vector<std::string> &curl_result::raw_headers() const {
	return storage->raw_headers;
}

const std::string &curl_result::body() const {
	return storage->body;
}

const std::string &curl_result::mime_type() const {
	return storage->mime_type;
}

/* ==== async::curl_performer ==== */
curl_performer::curl_performer(std::shared_ptr<curl_storage> storage_) : storage(storage_) {}

bool curl_performer::await_ready() {
	return storage->state >= storage->fire_at;
}

curl_result curl_performer::await_resume() {
	return curl_result{storage};
}

void curl_performer::await_suspend(std::coroutine_handle<> h) noexcept {
	storage->work = event_loop_work(h);
	if (storage->state == INITIAL) {
		storage->on_state_update(ENQUEUED, 0);
		curl_event_source::local->schedule_perform(storage);
	}
}

/* ==== async::curl_event_source::impl ==== */
/** @private */
struct curl_event_source::impl {
	ev_loop_t *loop;

	CURLM *curl;
	int curl_transfers_alive = 0;
	ev_with_arg<ev_timer> curl_timer;

	void process_curl_events() {
		int msgs_left;
		CURLMsg *msg;

		while ((msg = curl_multi_info_read(curl, &msgs_left))) {
			if (msg->msg == CURLMSG_DONE) {
				int res = msg->data.result;
				CURL *easy = msg->easy_handle;
				curl_storage *storage;
				curl_easy_getinfo(easy, CURLINFO_PRIVATE, &storage);
				curl_multi_remove_handle(curl, easy);
				storage->on_state_update(COMPLETED, res);
			}
		}
		if (curl_transfers_alive <= 0) {
			ev_timer_stop(loop, &curl_timer.w);
		}
	}

	static void curl_timer_cb(ev_loop_t *, ev_timer *w, int) {
		auto p = (impl *) ((ev_with_arg<ev_timer> *) w)->arg;

		curl_multi_socket_action(p->curl, CURL_SOCKET_TIMEOUT, 0, &p->curl_transfers_alive);
		p->process_curl_events();
	}

	static void curl_event_cb(ev_loop_t *, ev_io *w, int revents) {
		auto p = (impl *) ((ev_with_arg<ev_io> *) w)->arg;

		int action = 0;
		if (revents & EV_READ) {
			action |= CURL_POLL_IN;
		}
		if (revents & EV_WRITE) {
			action |= CURL_POLL_OUT;
		}
		curl_multi_socket_action(p->curl, w->fd, action, &p->curl_transfers_alive);
		p->process_curl_events();
	}

	static int curl_timer_function(CURLM *, long timeout, impl *p) {
		ev_timer_stop(p->loop, &p->curl_timer.w);
		if (timeout >= 0) {
			ev_timer_init(&p->curl_timer.w, curl_timer_cb, (double) (timeout) / 1000, 0.);
			ev_timer_start(p->loop, &p->curl_timer.w);
		}
		return 0;
	}

	static size_t cb_write(char *ptr, std::size_t, std::size_t len, curl_storage *storage) {
		if (storage->state < HEADERS_RECIEVED) {
			storage->on_state_update(HEADERS_RECIEVED, 0);
		}
		storage->write_body(ptr, len);
		return len;
	}

	static size_t cb_header(char *ptr, std::size_t, std::size_t len, curl_storage *storage) {
		std::size_t copy_len = len;
		if (len >= 2 && ptr[len - 1] == '\n' && ptr[len - 2] == '\r')
			copy_len -= 2;
		storage->raw_headers.emplace_back(ptr, copy_len);
		return len;
	}

	static int curl_socket_function(CURL *, curl_socket_t s, int what, impl *p,
									ev_with_arg<ev_io> *event) {
		if (what == CURL_POLL_REMOVE) {
			if (event) {
				ev_io_stop(p->loop, &event->w);
				delete event;
			}
		} else {
			if (event) {
				ev_io_stop(p->loop, &event->w);
			} else {
				event = new ev_with_arg<ev_io>;
				event->arg = p;
				curl_multi_assign(p->curl, s, event);
			}

			int kind = 0;
			if (what & CURL_POLL_IN)
				kind |= EV_READ;
			if (what & CURL_POLL_OUT)
				kind |= EV_WRITE;
			ev_io_init(&event->w, curl_event_cb, s, kind);
			ev_io_start(p->loop, &event->w);
		}
		return 0;
	}

	void on_init() {
		loop = (ev_loop_t *) libev_event_loop::get()->get_underlying_loop();
		curl = curl_multi_init();
		curl_multi_setopt(curl, CURLMOPT_SOCKETFUNCTION, curl_socket_function);
		curl_multi_setopt(curl, CURLMOPT_SOCKETDATA, this);
		curl_multi_setopt(curl, CURLMOPT_TIMERFUNCTION, curl_timer_function);
		curl_multi_setopt(curl, CURLMOPT_TIMERDATA, this);

		ev_timer_init(&curl_timer.w, curl_timer_cb, 0., 0.);
		curl_timer.arg = this;
		ev_timer_start(loop, &curl_timer.w);
	}

	void on_stop() {
		ev_timer_stop(loop, &curl_timer.w);
		curl_multi_cleanup(curl);
	}

	void schedule_perform(curl_storage *storage) {
		curl_easy_setopt(storage->handle, CURLOPT_WRITEFUNCTION, cb_write);
		curl_easy_setopt(storage->handle, CURLOPT_WRITEDATA, storage);
		curl_easy_setopt(storage->handle, CURLOPT_HEADERFUNCTION, cb_header);
		curl_easy_setopt(storage->handle, CURLOPT_HEADERDATA, storage);
		curl_easy_setopt(storage->handle, CURLOPT_PRIVATE, storage);
		curl_multi_add_handle(curl, storage->handle);
	}
};

/* ==== async::curl_event_source ==== */
thread_local std::shared_ptr<curl_event_source> curl_event_source::local;

void curl_event_source::bind_to_thread() {
	local = std::static_pointer_cast<curl_event_source, event_source>(shared_from_this());
}

void curl_event_source::on_init() {
	pimpl = new impl{};
	pimpl->on_init();
}

void curl_event_source::on_stop() {
	pimpl->on_stop();
	delete pimpl;
}

void curl_event_source::schedule_perform(std::shared_ptr<curl_storage> storage) {
	pimpl->schedule_perform(storage.get());
}