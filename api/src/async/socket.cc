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

inline void storage_listen_to(std::unique_ptr<socket_storage> &storage, socket_event_type mask) {
	if ((storage->event_mask & mask) != mask) {
		storage->event_mask = mask;
		libev_event_loop::get()->socket_mod(storage.get());
	}
}

coro<std::pair<async::socket, sockaddr>> socket::accept() {
	storage_listen_to(storage, READABLE);
	while (1) {
		sockaddr naddr{};
		socklen_t addrlen = sizeof(sockaddr);
		int nfd = ::accept4(storage->fd, &naddr, &addrlen, SOCK_NONBLOCK);
		if (nfd != -1) {
			co_return {socket{nfd}, naddr};
		}
		utils::ensure(!WOULDBLOCK, "accept failed");
		co_await socket_performer{READABLE, storage.get()};
	}
}

void socket::bind(const sockaddr *addr, socklen_t addrlen) {
	utils::ensure(::bind(storage->fd, addr, addrlen), "bind failed");
}

coro<void> socket::connect(const sockaddr *addr, socklen_t addrlen) {
	storage_listen_to(storage, WRITABLE);
	int result = ::connect(storage->fd, addr, addrlen);
	if (result != -1) {
		co_return;
	}
	utils::ensure(errno != EINPROGRESS, "connect failed");
	co_await socket_performer{WRITABLE, storage.get()};
	storage_listen_to(storage, READABLE);

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
	storage_listen_to(storage, READABLE);
	while (1) {
		ssize_t cnt = ::read(storage->fd, buf, size);
		if (cnt != -1) {
			co_return cnt;
		}
		utils::ensure(!WOULDBLOCK, "read failed");
		co_await socket_performer{READABLE, storage.get()};
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
	storage_listen_to(storage, WRITABLE);
	while (1) {
		ssize_t cnt = ::write(storage->fd, buf.data(), buf.size());
		if (cnt != -1) {
			co_return cnt;
		}
		utils::ensure(!WOULDBLOCK, "write failed");
		co_await socket_performer{WRITABLE, storage.get()};
	}
	storage_listen_to(storage, READABLE);
}

coro<void> socket::write_all(std::string_view buf) {
	storage_listen_to(storage, WRITABLE);
	while (buf.size()) {
		ssize_t curr = ::write(storage->fd, buf.data(), buf.size());
		if (curr == -1) {
			utils::ensure(!WOULDBLOCK, "write failed");
			continue;
		} else if (curr == 0) {
			throw std::runtime_error("no bytes were written to socket");
		} else {
			buf = buf.substr(curr);
		}
		co_await socket_performer{WRITABLE, storage.get()};
	}
	storage_listen_to(storage, READABLE);
}

void socket::close() {
	libev_event_loop::get()->socket_del(storage.get());
	::close(storage->fd);
	storage = nullptr;
}
