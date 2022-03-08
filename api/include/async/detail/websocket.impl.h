/* ==== async::websocket ==== */
template <typename T>
inline coro<T> websocket::expect() {
	T result;
	if (!result.ParseFromString(co_await read())) {
		throw websocket_error("invalid query");
	}
	co_return result;
}

template <Serializable T>
inline coro<void> websocket::respond(const typename T::initializable_type &response) {
	std::string data;
	T{response}.SerializeToString(&data);
	co_await write(data);
}

template <Serializable T>
inline coro<void> websocket::respond(const T &response) {
	std::string data;
	response.SerializeToString(&data);
	co_await write(data);
}

/* ==== async::websocket_performer ==== */
template <typename U>
inline void websocket_performer::await_suspend(std::coroutine_handle<U> &h) {
	return await_suspend(std::move(event_loop_work(h)));
}