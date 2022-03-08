#pragma once

#include "stdafx.h"

#include "event-loop.h"

namespace async {
namespace detail {
	class future_base {
		ONLY_DEFAULT_MOVABLE_CLASS(future_base)

	protected:
		enum { CLEAR, READY, AWAITING, FULFILLED } status;

		event_loop_work work;

		void set_ready() noexcept;

	public:
		future_base() {}

		bool await_ready() const noexcept;
		template <typename U>
		void await_suspend(std::coroutine_handle<U> &h) noexcept;

		void clear() noexcept;
		bool is_awaiting() const noexcept;
	};
}  // namespace detail

template <typename T>
class future final : public detail::future_base {
private:
	std::variant<std::exception_ptr, T> result;

public:
	T await_resume();

	void set_exception(const std::exception_ptr &exc) noexcept;
	void set_result(const T &t);
	void set_result(T &&t);
};

template <>
class future<void> final : public detail::future_base {
private:
	std::optional<std::exception_ptr> result;

public:
	void await_resume();

	void set_exception(const std::exception_ptr &exc) noexcept;
	void set_result() noexcept;
};

#include "detail/future.impl.h"
}  // namespace async