#include "async/socket.h"
using namespace async;

#include "utils/common.h"

/* ==== async::socket ==== */
socket::socket(int fd) {
	init(fd);
}

void socket::init(int fd) {
	storage = std::make_unique<socket_storage>();
	storage->fd = fd;
	libev_event_loop::get()->socket_add(storage.get());
}

socket::socket(int domain, int type, int protocol) {
	int fd = ::socket(domain, type, protocol);
	utils::ensure(fd == -1, "socket failed");
	init(fd);
}

socket::~socket() {
	if (!storage) {
		return;
	}
	close();
}

coro<async::socket> socket::connect(std::string_view node, std::string_view service,
									const addrinfo &hints) {
	addrinfo *result;
	utils::ensure(getaddrinfo(node.data(), service.data(), &hints, &result), "getaddrinfo failed");
	utils::scope_guard free_result([&] { freeaddrinfo(result); });

	for (addrinfo *rp = result; rp; rp = rp->ai_next) {
		try {
			auto s = async::socket(rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK, rp->ai_protocol);
			co_await s.connect(rp->ai_addr, rp->ai_addrlen);
			co_return s;
		} catch (...) {
			if (!rp->ai_next) {
				std::throw_with_nested(std::runtime_error("out of options connecting to " +
														  std::string(node) + ":" +
														  std::string(service)));
			}
		}
	}
}

#define WOULDBLOCK (errno == EAGAIN || errno == EWOULDBLOCK)

coro<std::pair<async::socket, sockaddr>> socket::accept() {
	while (1) {
		sockaddr naddr{};
		socklen_t addrlen = sizeof(sockaddr);
		int nfd = ::accept4(storage->fd, &naddr, &addrlen, SOCK_NONBLOCK);
		if (nfd != -1) {
			co_return {socket{nfd}, naddr};
		}
		utils::ensure(!WOULDBLOCK, "accept failed");
		co_await socket_performer<true>{storage.get()};
	}
}

void socket::bind(const sockaddr *addr, socklen_t addrlen) {
	utils::ensure(::bind(storage->fd, addr, addrlen), "bind failed");
}

coro<void> socket::connect(const sockaddr *addr, socklen_t addrlen) {
	int result = ::connect(storage->fd, addr, addrlen);
	if (result != -1) {
		co_return;
	}
	utils::ensure(errno != EINPROGRESS, "connect failed");
	co_await socket_performer<false>{storage.get()};

	int optval = 0;
	unsigned int optlen = 4;
	utils::ensure(getsockopt(storage->fd, SOL_SOCKET, SO_ERROR, &optval, &optlen),
				  "connect failed");
	errno = optval;
	utils::ensure(optval, "connect failed");
}

void socket::listen(int backlog) {
	utils::ensure(::listen(storage->fd, backlog), "listen failed");
}

coro<ssize_t> socket::read(char *buf, std::size_t size) {
	while (1) {
		ssize_t cnt = ::read(storage->fd, buf, size);
		if (cnt != -1) {
			co_return cnt;
		}
		utils::ensure(!WOULDBLOCK, "read failed");
		co_await socket_performer<true>{storage.get()};
	}
}

coro<void> socket::read_exactly(char *buf, std::size_t size) {
	std::size_t it = 0;
	while (it != size) {
		std::size_t curr = co_await read(buf + it, size - it);
		if (curr) {
			it += curr;
		} else {
			throw std::runtime_error("no bytes were read from socket");
		}
	}
}

coro<ssize_t> socket::write(std::string_view buf) {
	while (1) {
		ssize_t cnt = ::write(storage->fd, buf.data(), buf.size());
		if (cnt != -1) {
			co_return cnt;
		}
		utils::ensure(!WOULDBLOCK, "write failed");
		co_await socket_performer<false>{storage.get()};
	}
}

coro<void> socket::write_all(std::string_view buf) {
	while (buf.size()) {
		std::size_t curr = co_await write(buf);
		if (curr) {
			buf = buf.substr(curr);
		} else {
			throw std::runtime_error("no bytes were written to socket");
		}
	}
}

void socket::close() {
	libev_event_loop::get()->socket_del(storage.get());
	::close(storage->fd);
	storage = nullptr;
}
