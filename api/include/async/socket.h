#pragma once

#include "stdafx.h"

#include <netdb.h>
#include <sys/errno.h>
#include <sys/socket.h>

#include "coro.h"
#include "libev-event-loop.h"

namespace async {
// class socket;
// template <typename, typename...>
// class eagain_performer;

// class socket {
// private:
// 	int fd;
// 	socket_storage *storage;

// 	void init();
// 	void uninit();

// public:
// 	socket();
// 	explicit socket(int);
// 	socket(int, int, int);
// 	socket(socket &) = delete;
// 	socket(socket &&);
// 	~socket();

// 	socket &operator=(socket &&);

// #define DEFINE_SOCKET_FUNCTION(gname, mname, opt)                                             \
// 	template <typename... Args>                                                               \
// 	inline eagain_performer<decltype(gname), int, Args...> mname(                             \
// 		Args &&...args) requires std::invocable<decltype(gname), int, Args...> {              \
// 		using return_t = std::invoke_result_t<decltype(gname), int, Args...>;                 \
//                                                                                               \
// 		return_t res = gname(fd, std::forward<Args>(args)...);                                \
//                                                                                               \
// 		bool needs_repeat = false;                                                            \
// 		if constexpr (opt == 2)                                                               \
// 			needs_repeat = (errno == EINPROGRESS);                                            \
// 		else                                                                                  \
// 			needs_repeat = (errno == EAGAIN || errno == EWOULDBLOCK);                         \
// 		if (res != -1)                                                                        \
// 			needs_repeat = false;                                                             \
//                                                                                               \
// 		if (needs_repeat)                                                                     \
// 			return {storage, opt, gname, std::forward<int>(fd), std::forward<Args>(args)...}; \
// 		return eagain_performer<decltype(gname), int, Args...>{res == -1 ? -errno : res};     \
// 	}

// 	DEFINE_SOCKET_FUNCTION(::accept, accept, 0)
// 	DEFINE_SOCKET_FUNCTION(::connect, connect, 2)
// 	DEFINE_SOCKET_FUNCTION(::read, read, 0)
// 	DEFINE_SOCKET_FUNCTION(::recv, recv, 0)
// 	DEFINE_SOCKET_FUNCTION(::recvfrom, recvfrom, 0)
// 	DEFINE_SOCKET_FUNCTION(::recvmsg, recvmsg, 0)
// 	DEFINE_SOCKET_FUNCTION(::send, send, 1)
// 	DEFINE_SOCKET_FUNCTION(::sendmsg, sendmsg, 1)
// 	DEFINE_SOCKET_FUNCTION(::sendto, sendto, 1)
// 	DEFINE_SOCKET_FUNCTION(::write, write, 1)

// #undef DEFINE_SOCKET_FUNCTION

// 	coro<void> read_exactly(char *, std::size_t);
// 	coro<void> write_all(const char *, std::size_t);
// };

// template <typename Func, typename... Args>
// class eagain_performer {
// private:
// 	using return_t = std::invoke_result_t<Func, Args...>;

// 	socket_storage *storage;
// 	int flags;
// 	Func *func;
// 	std::optional<std::tuple<Args...>> args;
// 	std::optional<return_t> ret;

// 	template <int... Is>
// 	auto invoke2(detail::index<Is...>) -> return_t;
// 	bool invoke();

// public:
// 	explicit eagain_performer(return_t res);
// 	eagain_performer(socket_storage *storage, int flags, Func *, Args &&...);

// 	bool await_ready();
// 	auto await_resume() -> return_t;
// 	template <typename T>
// 	void await_suspend(std::coroutine_handle<T> &);
// };

// #include "detail/socket.impl.h"
}  // namespace async