/* ==== async::detail::future_base ===== */
template <typename U>
inline void detail::future_base::await_suspend(std::coroutine_handle<U> &h) noexcept {
	status = AWAITING;
	work = event_loop_work(h);
}

/* ==== async::future<T> ==== */
template <typename T>
inline T future<T>::await_resume() {
	status = FULFILLED;
	if (result.index() == 0) {
		std::rethrow_exception(get<0>(result));
	}
	return get<1>(result);
}

template <typename T>
inline void future<T>::set_exception(const std::exception_ptr &exc) noexcept {
	result = exc;
	set_ready();
}

template <typename T>
inline void future<T>::set_result(const T &t) {
	result = t;
	set_ready();
}

template <typename T>
inline void future<T>::set_result(T &&t) {
	result = std::move(t);
	set_ready();
}
