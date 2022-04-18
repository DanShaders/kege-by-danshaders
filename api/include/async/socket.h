#pragma once

#include "stdafx.h"

#include <netdb.h>
#include <sys/errno.h>
#include <sys/socket.h>

#include "coro.h"
#include "libev-event-loop.h"

namespace async {
class socket;
template <bool is_awaiting_read>
struct socket_performer;

class socket {
	ONLY_DEFAULT_MOVABLE_CLASS(socket)

private:
	std::unique_ptr<socket_storage> storage;

	explicit socket(int fd);
	void init(int fd);

public:
	socket() {}

	socket(int domain, int type, int protocol);
	~socket();

	static coro<async::socket> connect(std::string_view node, std::string_view service,
									   const addrinfo &hints = {.ai_socktype = SOCK_STREAM});

	coro<std::pair<socket, sockaddr>> accept();
	void bind(const sockaddr *addr, socklen_t addrlen);
	coro<void> connect(const sockaddr *addr, socklen_t addrlen);
	void listen(int backlog);

	coro<ssize_t> read(char *buf, std::size_t size);
	coro<void> read_exactly(char *buf, std::size_t size);

	coro<ssize_t> write(std::string_view buf);
	coro<void> write_all(std::string_view buf);

	void close();
};

template <bool is_awaiting_read>
struct socket_performer {
	socket_storage *storage = nullptr;

	bool await_ready() {
		return false;
	}

	void await_resume() {}

	template <typename T>
	void await_suspend(std::coroutine_handle<T> &h) {
		if constexpr (is_awaiting_read) {
			storage->read_work = event_loop_work(&h);
		} else {
			storage->write_work = event_loop_work(&h);
		}
	}
};
}  // namespace async