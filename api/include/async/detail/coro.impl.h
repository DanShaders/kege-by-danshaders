/* ==== async::coro<T>::base_promise_type */
template <typename T>
inline coro<T> coro<T>::base_promise_type::get_return_object() {
	handle = coro_handle::from_promise(*(promise_type *) this);
	return {(promise_type *) this};
}

template <typename T>
inline std::suspend_always coro<T>::base_promise_type::final_suspend() noexcept {
	auto func = (void (*)(void *))((&handle)->*(&std::coroutine_handle<promise_type>::destroy));
	final_suspend_inner((void *) &handle, func);
	return {};
}

/* ==== async::coro<T>::void_promise_type ==== */
template <typename T>
inline void coro<T>::void_promise_type::return_void() {}

/* ==== async::coro<T>::value_promise_type ==== */
template <typename T>
inline void coro<T>::value_promise_type::return_value(T &&value) {
	this->ret_val = std::move(value);
}

template <typename T>
inline void coro<T>::value_promise_type::return_value(const T &value) {
	this->ret_val = value;
}

/* ==== async::coro<T> ==== */
template <typename T>
inline coro<T>::coro(promise_type *promise_) : promise(promise_) {}

template <typename T>
inline coro<T>::coro(coro &&other) {
	promise = other.promise;
	other.promise = 0;
}

template <typename T>
inline coro<T>::~coro() {
	if (!promise)
		return;
	assert(((void) "coroutine has not been awaited [enough]", promise->handle.done()));
	promise->handle.destroy();
}

template <typename T>
coro<T> &coro<T>::operator=(coro<T> &&other) {
	promise = other.promise;
	other.promise = 0;
	return *this;
}

template <typename T>
inline bool coro<T>::await_ready() {
	return promise->handle.done();
}

template <typename T>
inline T coro<T>::await_resume() {
	assert(promise->handle.done());
	if (promise->exc.has_value())
		std::rethrow_exception(promise->exc.value());
	assert(promise->handle.done());
	if constexpr (!std::is_same_v<T, void>)
		return std::move(promise->ret_val.value());
}

template <typename T>
template <typename U>
inline void coro<T>::await_suspend(std::coroutine_handle<U> &h) {
	promise->parent = std::move(event_loop_work(&h));
}

/* ==== async::suspend_t ==== */
template <typename T>
inline void suspend_t::await_suspend(std::coroutine_handle<T> &h) noexcept {
	event_loop::local->schedule_work(event_loop_work(&h));
}

/* ==== async::detail ==== */
namespace detail {
bool run_until_complete(int);

template <typename T, typename... U>
inline bool run_until_complete(int alive, coro<T> &&c, U... args) {
	bool flag = true;
	c.promise->is_top_level = true;
	if (c.promise->handle.done()) {
		if (c.promise->exc.has_value()) {
			event_loop::local->handle_exception(c.promise->exc.value());
			flag = false;
		}
	} else {
		++alive;
	}
	return run_until_complete(alive, std::forward<U>(args)...) && flag;
}
}  // namespace detail

/* ==== async ==== */
template <typename... U>
inline bool run_until_complete(coro<U> &&...args) {
	return detail::run_until_complete(0, std::forward<coro<U>>(args)...);
}

template <typename T>
void schedule_detached(coro<T> &&c) {
	if (!c.promise->handle.done()) {
		c.promise->is_top_level = true;
		c.promise->destroy_on_done = true;
		c.promise = 0;
		++async::event_loop::local->alive_coroutines;
	}
}
