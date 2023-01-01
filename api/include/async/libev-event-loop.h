#pragma once

#include "stdafx.h"

#include "event-loop.h"

namespace async {
class sleep;
struct socket_storage;
class libev_event_loop;

class sleep {
private:
  friend class libev_event_loop;

  double duration;
  event_loop_work work;

public:
  sleep(std::chrono::nanoseconds duration_);

  bool await_ready();
  void await_resume();
  void await_suspend(std::coroutine_handle<> h);
};

/**
 * Implementation of the event loop interface using libev
 */
class libev_event_loop : public event_loop {
private:
  struct impl;
  impl* pimpl;

public:
  /**
   * Returns libev_event_loop bound to current thread
   */
  static std::shared_ptr<libev_event_loop> get();

  libev_event_loop();
  ~libev_event_loop();

  /**
   * Returns the underlying libev loop (pointer to `struct ev_loop`)
   */
  void* get_underlying_loop();

  bool run(bool until_complete) override;
  void stop_notify() override;
  void schedule_work(event_loop_work&& work) override;
  void handle_exception(std::exception_ptr const& exc) override;

  void register_source(std::shared_ptr<event_source> source) override;

  void bind_to_thread() override;

  /**
   * Schedules work from @ref sleep object for execution.
   */
  void schedule_timer_resume(sleep* obj);

  /**
   * Adds socket to the event loop.
   *
   * Populates @ref socket_storage::event with a pointer to libev's event structure.
   *
   * TODO: move socket_* function family to @ref event_loop
   */
  void socket_add(socket_storage* storage);

  /**
   * Removes socket from the event loop.
   */
  void socket_del(socket_storage* storage);

  /**
   * Updates the mask of listened events corresponding to @ref socket_storage::event_mask
   */
  void socket_mod(socket_storage* storage);
};

namespace detail {
  /** @private */
  template <typename T, typename U = void*>
  struct ev_with_arg {
    T w;
    U arg;
  };
}  // namespace detail
}  // namespace async
