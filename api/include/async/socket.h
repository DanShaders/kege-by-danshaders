#pragma once

#include "stdafx.h"

#include <netdb.h>
#include <sys/errno.h>
#include <sys/socket.h>

#include "coro.h"
#include "libev-event-loop.h"

namespace async {
class socket;
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

struct socket_performer {
	socket_event_type type = SOCK_NONE;
	socket_storage *storage = nullptr;

	bool await_ready() {
		return false;
	}

	socket_event_type await_resume() {
		return storage->last_event;
	}

	void await_suspend(std::coroutine_handle<> h);
};
}  // namespace async