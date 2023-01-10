#pragma once

#include "stdafx.h"

#include "event-loop.h"

namespace async {
namespace detail {
  /** @private */
  class future_base {
    ONLY_DEFAULT_MOVABLE_CLASS(future_base)

  protected:
    enum { CLEAR, READY, AWAITING, FULFILLED } status = CLEAR;

    event_loop_work work;

    void set_ready() noexcept;

  public:
    future_base() {}

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> h) noexcept;

    void clear() noexcept;
    bool is_awaiting() const noexcept;
  };
}  // namespace detail

template <typename T>
class future final : public detail::future_base {
private:
  std::variant<std::exception_ptr, T> result;

public:
  T await_resume() {
    status = FULFILLED;
    if (result.index() == 0) {
      std::rethrow_exception(get<0>(result));
    }
    return get<1>(result);
  }

  void set_exception(std::exception_ptr const& exc) noexcept {
    result = exc;
    set_ready();
  }

  void set_result(T const& t) {
    result = t;
    set_ready();
  }

  void set_result(T&& t) {
    result = std::move(t);
    set_ready();
  }
};

template <>
class future<void> final : public detail::future_base {
private:
  std::optional<std::exception_ptr> result;

public:
  void await_resume();

  void set_exception(std::exception_ptr const& exc) noexcept;
  void set_result() noexcept;
};
}  // namespace async
