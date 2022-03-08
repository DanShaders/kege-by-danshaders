#pragma once

#include "stdafx.h"
#include "config.h"

#include "coro.h"
#include "libev-event-loop.h"

namespace async {
class websocket;
class websocket_performer;
class websocket_flush_performer;
class websocket_read_performer;
class websocket_server;
class websocket_error;

using websocket_handler_t = std::function<void(websocket *)>;

template <typename T>
concept Serializable = requires(T t, std::string *s) {
	t.SerializeToString(s);
};

class websocket {
private:
	friend websocket_server;
	friend websocket_performer;
	friend websocket_flush_performer;
	friend websocket_read_performer;

	struct session;
	session *s;

	websocket(session *s_);
	void ensure_not_closed();

public:
	websocket_read_performer read();
	websocket_flush_performer write(const std::string_view &data);
	void write_buffered(const std::string_view &data);
	websocket_flush_performer flush();
	void schedule_flush();

	template <typename T>
	coro<T> expect();

	template <Serializable T>
	coro<void> respond(const typename T::initializable_type &response);

	template <Serializable T>
	coro<void> respond(const T &response);

	void close();
	void finish();
};

class websocket_performer {
protected:
	friend websocket;

	enum req_type { READ = 1, FLUSH = 2 } type;

	websocket *ws;

	websocket_performer(req_type type_, websocket *ws_);

public:
	bool await_ready();
	void await_suspend(event_loop_work &&work);

	template <typename U>
	void await_suspend(std::coroutine_handle<U> &h);
};

class websocket_read_performer : public websocket_performer {
public:
	websocket_read_performer(websocket *ws_);

	std::string await_resume();
};

class websocket_flush_performer : public websocket_performer {
public:
	websocket_flush_performer(websocket *ws_);

	void await_resume();
};

class websocket_server {
private:
	struct impl;
	impl *pimpl;

public:
	websocket_server(const config::hl_socket_address &addr, const websocket_handler_t &handler);
	~websocket_server();

	void add_loop(std::shared_ptr<event_loop> loop);
	void serve();
	void stop_notify();
};

class websocket_error : public std::runtime_error {
	using runtime_error::runtime_error;
};

#include "detail/websocket.impl.h"
}  // namespace async