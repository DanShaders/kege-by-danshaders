/* ==== async::event_loop_work ==== */
template <typename T>
inline event_loop_work::event_loop_work(std::coroutine_handle<T> *handle) {
	payload = resume_coroutine{
		handle,
		(decltype(resume_coroutine::done_func)) (handle->*(&std::coroutine_handle<T>::done)),
		(decltype(resume_coroutine::resume_func)) (handle->*(&std::coroutine_handle<T>::resume))};
}
