/* ==== async::socket ==== */
inline void socket::init() {
	if (fd == -1)
		return;
	storage->fd = fd;
	storage->flags = 0;
	libev_event_loop::get()->socket_add(storage);
}

inline void socket::uninit() {
	if (fd != -1) {
		libev_event_loop::get()->socket_del(storage);
		libev_event_loop::get()->schedule_socket_close(fd);
		fd = -1;
	}
	if (storage) {
		storage = 0;
	}
}

inline socket::socket() : socket(-1) {}

inline socket::socket(int fd_) : fd(fd_), storage(new socket_storage{}) {
	init();
}

inline socket::socket(int domain, int type, int protocol) {
	fd = ::socket(domain, type, protocol);
	if (fd == -1)
		throw std::system_error(errno, std::system_category(), "socket creation failed");
	storage = new socket_storage{};
	init();
}

inline socket::socket(socket &&other) {
	uninit();
	storage = other.storage, other.storage = 0;
	fd = other.fd, other.fd = -1;
}

inline socket::~socket() {
	uninit();
}

inline socket &socket::operator=(socket &&other) {
	uninit();
	storage = other.storage, other.storage = 0;
	fd = other.fd, other.fd = -1;
	return *this;
}

inline coro<void> socket::read_exactly(char *buffer, std::size_t sz) {
	std::size_t it = 0;
	while (it != sz) {
		std::size_t curr = co_await read(buffer + it, sz - it);
		if (!curr)
			throw std::runtime_error("socket::read is expected to read at least one byte");
		it += curr;
	}
}

inline coro<void> socket::write_all(const char *buffer, std::size_t sz) {
	std::size_t it = 0;
	while (it != sz) {
		std::size_t curr = co_await write(buffer + it, sz - it);
		if (!curr)
			throw std::runtime_error("socket::write is expected to write at least one byte");
		it += curr;
	}
}

/* ==== async::eagain_performer ==== */
template <typename Func, typename... Args>
template <int... Is>
inline auto eagain_performer<Func, Args...>::invoke2(detail::index<Is...>) -> return_t {
	return func(std::get<Is>(args.value())...);
}

template <typename Func, typename... Args>
inline bool eagain_performer<Func, Args...>::invoke() {
	return_t r = -1;
	bool needs_repeat = true;
	r = invoke2(detail::gen_seq<sizeof...(Args)>());
	needs_repeat = errno == EAGAIN || errno == EWOULDBLOCK;
	if (flags == 2)
		needs_repeat = errno == EINPROGRESS;
	if (r != -1)
		needs_repeat = false;

	if (!needs_repeat) {
		ret = (r == -1) ? -errno : r;
	}
	return !needs_repeat;
}

template <typename Func, typename... Args>
inline eagain_performer<Func, Args...>::eagain_performer(return_t ret_) : storage(0) {
	ret = ret_;
}

template <typename Func, typename... Args>
inline eagain_performer<Func, Args...>::eagain_performer(socket_storage *storage_, int flags_,
														 Func *func_, Args &&...args_)
	: storage(storage_), flags(flags_), func(func_), args({std::forward<Args>(args_)...}) {
	storage->sock_func =
		(decltype(socket_storage::sock_func)) (this->*(&eagain_performer<Func, Args...>::invoke));
	storage->sock_this = this;
	int needed_flags = flags == 0 ? EV_READ : EV_WRITE;
	if (storage->flags != needed_flags) {
		storage->flags = needed_flags;
		libev_event_loop::get()->socket_mod(storage);
	}
}

template <typename Func, typename... Args>
inline bool eagain_performer<Func, Args...>::await_ready() {
	return ret.has_value();
}

template <typename Func, typename... Args>
auto eagain_performer<Func, Args...>::await_resume() -> return_t {
	return_t value = ret.value();
	if (storage) {
		storage->flags = 0;
		libev_event_loop::get()->socket_mod(storage);
	}
	if (value < 0)
		throw std::system_error((int) -value, std::system_category(), "socket operation failed");
	return value;
}

template <typename Func, typename... Args>
template <typename T>
void eagain_performer<Func, Args...>::await_suspend(std::coroutine_handle<T> &h) {
	storage->work = event_loop_work(&h);
}
