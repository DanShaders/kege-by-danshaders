/* ==== async::sleep ==== */
template <typename T>
inline void sleep::await_suspend(std::coroutine_handle<T> &h) {
	work = event_loop_work(&h);
	libev_event_loop::get()->schedule_timer_resume(this);
}