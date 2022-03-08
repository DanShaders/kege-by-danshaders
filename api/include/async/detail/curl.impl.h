/* ==== async::curl ==== */
template <typename T>
inline CURLcode curl::set_opt(CURLoption opt, T value) {
	return curl_easy_setopt(get_handle(), opt, value);
}

/* ==== async::curl_performer ==== */
template <typename T>
inline void curl_performer::await_suspend(std::coroutine_handle<T> &h) noexcept {
	await_suspend(event_loop_work(&h));
}
