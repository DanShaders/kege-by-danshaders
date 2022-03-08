template <typename T>
void mutex::locker::await_suspend(std::coroutine_handle<T> &h) noexcept {
	await_suspend({h});
}
