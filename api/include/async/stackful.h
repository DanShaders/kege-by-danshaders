#pragma once

#include "stdafx.h"

#include "coro.h"

namespace async {
class coro_stack {
public:
  const static std::size_t STACK_SIZE = (512 << 10);  // 512KiB
  const static std::size_t STACK_LEN = STACK_SIZE / sizeof(uint64_t);

  std::shared_ptr<uint64_t[]> stack;

  coro_stack();
  coro_stack(coro_stack&) = delete;
  coro_stack(coro_stack&&) noexcept = default;

  coro_stack& operator=(coro_stack&) = delete;
  coro_stack& operator=(coro_stack&&) noexcept = default;
};

template <typename Func, typename... Args>
class stackful {
private:
  using return_t = std::invoke_result_t<Func, Args...>;

  void* context;
  Func* func;
  std::tuple<Args...> args;
  std::optional<return_t> ret_val;
  std::optional<std::exception_ptr> exc;
  bool ret_ready = false;
  event_loop_work resume_work;

  template <std::size_t... Is>
  void invoke2(std::index_sequence<Is...>);
  void invoke();

public:
  stackful(coro_stack const& stack, Func&&, Args&&...);
  stackful(stackful<Func, Args...>&) = delete;
  stackful(stackful<Func, Args...>&&) = delete;
  ~stackful();

  bool await_ready();
  return_t await_resume();
  void await_suspend(std::coroutine_handle<> h);
};

template <typename Awaitable>
auto stackful_await(Awaitable&& c) -> decltype(std::declval<Awaitable&>().await_resume());

#include "detail/stackful.impl.h"
}  // namespace async