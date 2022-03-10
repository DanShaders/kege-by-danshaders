/* ==== async::stackful ==== */
namespace detail {
struct stackful_context {
	uint64_t reserved[20];
#ifdef KEGE_SANITIZE_ADDRESS
	void *stack_bottom;
	std::size_t stack_size;
#endif
	std::shared_ptr<uint64_t[]> stack;
};

static thread_local std::vector<coro_stack> stack_poll;

#ifdef KEGE_SANITIZE_ADDRESS
void stackful_asan_start_switch(void **, void *, std::size_t);
void stackful_asan_finish_switch(void *, void **, std::size_t *);
#endif

thread_local inline void *current_stackful_context = 0;

void stackful_done();
void stackful_suspend(void *);
void stackful_resume(void *);
}  // namespace detail

template <typename Func, typename... Args>
template <std::size_t... Is>
inline void stackful<Func, Args...>::invoke2(std::index_sequence<Is...>) {
	try {
		if constexpr (std::is_same_v<void, return_t>) {
			func(std::get<Is>(args)...);
		} else {
			ret_val = std::move(func(std::get<Is>(args)...));
		}
	} catch (...) { exc = std::current_exception(); }
	ret_ready = true;
	if (resume_work.has_work()) {
		event_loop::local->schedule_work(std::move(resume_work));
	}
#ifdef KEGE_SANITIZE_ADDRESS
	auto ctx = (detail::stackful_context *) context;
	detail::stackful_asan_start_switch(0, ctx->stack_bottom, ctx->stack_size);
#endif
}

template <typename Func, typename... Args>
inline void stackful<Func, Args...>::invoke() {
#ifdef KEGE_SANITIZE_ADDRESS
	auto ctx = (detail::stackful_context *) context;
	detail::stackful_asan_finish_switch(0, &(ctx->stack_bottom), &(ctx->stack_size));
#endif
	invoke2(std::index_sequence_for<Args...>{});
}

template <typename Func, typename... Args>
inline stackful<Func, Args...>::stackful(const coro_stack &stack, Func &&func_, Args &&...args_)
	: func(std::forward<Func>(func_)), args(std::forward<Args>(args_)...) {
	using namespace detail;

	auto ctx = new stackful_context;

	ctx->stack = stack.stack;

	context = ctx;

	uint64_t irsp = (((uint64_t(ctx->stack.get() + coro_stack::STACK_LEN) - 16) & ~15ull) - 8);
	ctx->reserved[/* rip */ 0x38 / 8] =
		(uint64_t) (void *) (this->*(&async::stackful<Func, Args...>::invoke));
	ctx->reserved[/* rsp */ 0x48 / 8] = irsp;
	ctx->reserved[/* this */ 0x78 / 8] = (uint64_t) this;
	*((uint64_t *) (irsp)) = (uint64_t) &stackful_done;
	*((uint64_t *) (irsp + 8)) = (uint64_t) ctx;
	stackful_resume(context);
}

template <typename Func, typename... Args>
inline stackful<Func, Args...>::~stackful() {
	auto ctx = (detail::stackful_context *) context;
	assert(((void) "stackful coroutine has not been awaited [enough]", ret_ready));
	delete ctx;
}

template <typename Func, typename... Args>
inline bool stackful<Func, Args...>::await_ready() {
	return ret_ready;
}

template <typename Func, typename... Args>
inline stackful<Func, Args...>::return_t stackful<Func, Args...>::await_resume() {
	if (exc.has_value())
		std::rethrow_exception(exc.value());
	if constexpr (!std::is_same_v<void, return_t>)
		return std::move(ret_val.value());
}

template <typename Func, typename... Args>
template <typename T>
inline void stackful<Func, Args...>::await_suspend(std::coroutine_handle<T> &h) {
	resume_work = event_loop_work(&h);
}

/* ==== async ==== */
template <typename Awaitable>
auto stackful_await(Awaitable &&c) -> decltype(std::declval<Awaitable &>().await_resume()) {
	if (!c.await_ready()) {
		void *ptr = detail::current_stackful_context;
		c.await_suspend(event_loop_work(detail::stackful_resume, ptr));
		detail::stackful_suspend(ptr);
	}
	if constexpr (std::is_same_v<decltype(std::declval<Awaitable &>().await_resume()), void>) {
		c.await_resume();
	} else {
		return c.await_resume();
	}
}
