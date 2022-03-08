#include "async/websocket.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <libwebsockets.h>
#pragma GCC diagnostic pop

#include <sys/stat.h>

#include "logging.h"

using namespace async;

/* ==== async::websocket ==== */

struct websocket::session {
	using list_t = std::list<websocket::session *>;

	static thread_local list_t by_thread;
	static thread_local int alive_sessions;
	static thread_local std::atomic_flag notification_wanted, has_alive_sessions;

	websocket *ws;
	lws *wsi;
	std::deque<std::pair<std::shared_ptr<char[]>, int>> write_msg;
	std::deque<std::string> read_msg;
	event_loop_work flush_done_resume, msg_read_resume;
	list_t::iterator list_iter;
	bool read_msg_is_finished = true;
	std::optional<lws_close_status> close_requested;

	~session() {
		if (ws) {
			ws->s = nullptr;
		}
		flush_done_resume.do_work();
		msg_read_resume.do_work();
		websocket::session::by_thread.erase(list_iter);
		--alive_sessions;
		if (!alive_sessions) {
			has_alive_sessions.clear();
			if (notification_wanted.test()) {
				has_alive_sessions.notify_all();
			}
		}
	}
};

thread_local websocket::session::list_t websocket::session::by_thread;
thread_local int websocket::session::alive_sessions;
thread_local std::atomic_flag websocket::session::notification_wanted,
	websocket::session::has_alive_sessions;

websocket::websocket(session *s_) : s(s_) {
	if (s) {
		s->ws = this;
	}
}

void websocket::ensure_not_closed() {
	if (!s) {
		throw websocket_error("websocket is closed");
	}
}

websocket_read_performer websocket::read() {
	ensure_not_closed();
	return websocket_read_performer{this};
}

websocket_flush_performer websocket::write(const std::string_view &data) {
	write_buffered(data);
	return flush();
}

void websocket::write_buffered(const std::string_view &data) {
	ensure_not_closed();
	char *msg = new char[LWS_PRE + data.size()];
	std::copy(data.begin(), data.end(), msg + LWS_PRE);
	s->write_msg.push_back({std::unique_ptr<char[]>{msg}, data.size()});
}

websocket_flush_performer websocket::flush() {
	ensure_not_closed();
	return websocket_flush_performer{this};
}

void websocket::schedule_flush() {
	ensure_not_closed();
	lws_callback_on_writable(s->wsi);
}

void websocket::close() {
	ensure_not_closed();
	s->ws = 0;
	s->close_requested = LWS_CLOSE_STATUS_NORMAL;
	lws_callback_on_writable(s->wsi);
	s = 0;
}

void websocket::finish() {
	if (s) {
		s->ws = 0;
		if (!s->close_requested) {
			s->close_requested = LWS_CLOSE_STATUS_NORMAL;
			lws_callback_on_writable(s->wsi);
		}
	}
	delete this;
}

/* ==== async::websocket_performer ==== */
websocket_performer::websocket_performer(req_type type_, websocket *ws_) : type(type_), ws(ws_) {}

bool websocket_performer::await_ready() {
	ws->ensure_not_closed();

	if (type == READ) {
		return ws->s->read_msg.size();
	} else {
		return ws->s->write_msg.empty();
	}
}

void websocket_performer::await_suspend(event_loop_work &&work) {
	(type == READ ? ws->s->msg_read_resume : ws->s->flush_done_resume) = std::move(work);
	lws_callback_on_writable(ws->s->wsi);
}

/* ==== async::websocket_read_performer ==== */
websocket_read_performer::websocket_read_performer(websocket *ws_)
	: websocket_performer(READ, ws_) {}

std::string websocket_read_performer::await_resume() {
	ws->ensure_not_closed();
	std::string result = ws->s->read_msg.front();
	ws->s->read_msg.pop_front();
	return result;
}

/* ==== async::websocket_flush_performer ==== */
websocket_flush_performer::websocket_flush_performer(websocket *ws_)
	: websocket_performer(FLUSH, ws_) {}

void websocket_flush_performer::await_resume() {
	ws->ensure_not_closed();
}

/* ==== async::websocket_server::impl */
struct websocket_server::impl {
	std::vector<std::shared_ptr<libev_event_loop>> loops;
	lws_context *context;
	config::hl_socket_address addr;
	websocket_handler_t handler;
	std::atomic_flag stop_requested;

	impl(const config::hl_socket_address &addr_, const websocket_handler_t &handler_)
		: addr(addr_), handler(handler_) {}

	void add_loop(std::shared_ptr<event_loop> loop) {
		loops.push_back(std::dynamic_pointer_cast<libev_event_loop>(loop));
	}

	static int callback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len) {
		if (reason == LWS_CALLBACK_HTTP) {
			return -1;
		}
		auto p = (impl *) lws_context_user(lws_get_context(wsi));
		auto s = (websocket::session *) user;
		switch (reason) {
			case LWS_CALLBACK_ESTABLISHED: {
				if (p->stop_requested.test()) {
					return -1;
				}

				new (s) websocket::session{};

				++websocket::session::alive_sessions;
				websocket::session::has_alive_sessions.test_and_set();
				websocket::session::by_thread.push_front(s);
				s->list_iter = websocket::session::by_thread.begin();
				s->wsi = wsi;

				p->handler(new websocket{s});
				break;
			}

			case LWS_CALLBACK_CLOSED: {
				s->~session();
				break;
			}

			case LWS_CALLBACK_SERVER_WRITEABLE: {
				if (s->write_msg.size()) {
					auto [msg, msg_sz] = s->write_msg.front();
					char *raw_msg = msg.get();
					lws_write(wsi, (unsigned char *) (raw_msg + LWS_PRE), msg_sz, LWS_WRITE_BINARY);
					s->write_msg.pop_front();
					if (s->write_msg.empty()) {
						s->flush_done_resume.do_work();
					}
				} else if (s->close_requested) {
					lws_close_reason(wsi, s->close_requested.value(), 0, 0);
					if (s->ws) {
						s->ws->s = 0;
						s->ws = 0;
					}
					return -1;
				}
				if (s->write_msg.size() || s->close_requested) {
					lws_callback_on_writable(wsi);
				}
				break;
			}

			case LWS_CALLBACK_RECEIVE: {
				bool &finished = s->read_msg_is_finished;
				if (finished) {
					s->read_msg.push_back({(char *) in, (char *) in + len});
					finished = false;
				} else {
					s->read_msg.back() += std::string_view{(char *) in, (char *) in + len};
				}

				if (lws_is_final_fragment(wsi)) {
					finished = true;
					s->msg_read_resume.do_work();
				}
				break;
			}

			default:
				break;
		}
		return 0;
	}

	void serve() {
		std::vector<void *> raw_loops;
		for (auto loop : loops) {
			raw_loops.push_back(loop->get_underlying_loop());
		}

		// clang-format off
		lws_protocols protocols[] = {{
			.name = "ws",
			.callback = callback,
			.per_session_data_size = sizeof(websocket::session),
			.rx_buffer_size = 4096
			},
		{}};

		lws_retry_bo_t retry = {
			.secs_since_valid_ping = 3,
			.secs_since_valid_hangup = 10,
		};

		lws_context_creation_info info = {
			.protocols = protocols,
			.options = LWS_SERVER_OPTION_LIBEV,
			.user = this,
			.count_threads = (unsigned) raw_loops.size(),
			.foreign_loops = &raw_loops[0],
			.retry_and_idle_policy = &retry,
		};
		// clang-format on

		if (addr.use_unix_sockets) {
			info.iface = addr.path.c_str();
			info.options |= LWS_SERVER_OPTION_UNIX_SOCK;
		} else {
			info.port = addr.port;
		}

		lws_set_log_level(LLL_ERR | LLL_WARN, 0);
		context = lws_create_context(&info);
		if (!context) {
			throw websocket_error("unable to create websocket context");
		}
		if (addr.use_unix_sockets && addr.perms != -1) {
			chmod(addr.path.c_str(), addr.perms);
		}
	}

	void stop_notify() {
		stop_requested.test_and_set();
		std::vector<completion_token> tokens;
		std::vector<std::atomic_flag *> alive_sessions(loops.size());
		for (size_t i = 0; i < loops.size(); ++i) {
			tokens.push_back(loops[i]->schedule_work_ts(event_loop_work([&, i] {
				alive_sessions[i] = &websocket::session::has_alive_sessions;
				websocket::session::notification_wanted.test_and_set();
				for (auto s : websocket::session::by_thread) {
					s->close_requested = LWS_CLOSE_STATUS_GOINGAWAY;
					lws_callback_on_writable(s->wsi);
				}
			})));
		}
		for (auto token : tokens) {
			token.wait();
		}
		for (auto awaiter : alive_sessions) {
			while (awaiter->test()) {
				awaiter->wait(true);
			}
		}
		lws_context_destroy(context);
	}
};

/* ==== async::websocket_server ==== */
websocket_server::websocket_server(const config::hl_socket_address &addr,
								   const websocket_handler_t &handler)
	: pimpl(new impl{addr, handler}) {}

websocket_server::~websocket_server() {
	delete pimpl;
}

void websocket_server::add_loop(std::shared_ptr<event_loop> loop) {
	return pimpl->add_loop(loop);
}

void websocket_server::serve() {
	pimpl->serve();
}

void websocket_server::stop_notify() {
	pimpl->stop_notify();
}
