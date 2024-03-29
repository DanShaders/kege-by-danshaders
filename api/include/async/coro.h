#pragma once

#include "stdafx.h"

#include "event-loop.h"

namespace stacktrace {
// src/stacktrace.cc
void async_update_stacktrace(std::exception_ptr const& ptr);
}  // namespace stacktrace

namespace async {
template <typename>
class coro;
class suspend_t;

namespace detail {
  /** @private */
  struct promise_type_facade {
    std::optional<std::exception_ptr> exc;
    std::optional<event_loop_work> parent;

    bool is_top_level : 1 = false;
    bool destroy_on_done : 1 = false;
    bool suspends_parent : 1 = false;

    std::suspend_never initial_suspend() noexcept;
    void final_suspend_inner(std::coroutine_handle<> h) noexcept;
    void unhandled_exception();
  };
}  // namespace detail

template <typename T>
class [[nodiscard]] coro {
public:
  struct base_promise_type;
  struct void_promise_type;
  struct value_promise_type;

  using promise_type =
      std::conditional<std::is_same_v<T, void>, void_promise_type, value_promise_type>::type;
  using coro_handle = std::coroutine_handle<promise_type>;

  /** @private */
  struct base_promise_type : detail::promise_type_facade {
    coro_handle handle;

    coro<T> get_return_object();
    std::suspend_always final_suspend() noexcept;
  };

  /** @private */
  struct void_promise_type : base_promise_type {
    void return_void();
  };

  /** @private */
  struct value_promise_type : base_promise_type {
    std::optional<T> ret_val;

    void return_value(T&&);
    void return_value(T const&);
  };

  promise_type* promise;

  coro(promise_type*);
  coro(coro&) = delete;
  coro(coro&&);
  ~coro();

  coro<T>& operator=(coro<T>&& other);

  bool await_ready();
  T await_resume();
  void await_suspend(std::coroutine_handle<> h);
};

/** @private */
class suspend_t {
public:
  bool await_ready() noexcept {
    return false;
  }

  void await_resume() noexcept {}

  void await_suspend(std::coroutine_handle<> h) noexcept;
} inline suspend;

template <typename... U>
bool run_until_complete(coro<U>&&...);

template <typename T>
void schedule_detached(coro<T>&& c);

#include "detail/coro.impl.h"
}  // namespace async
