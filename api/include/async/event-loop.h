/**
 * Interfaces for event loops.
 * @file
 */
#pragma once

#include "stdafx.h"

/** Namespace for all asynchronous stuff. */
namespace async {
class event_loop_work;
class event_loop;
class event_source;

/**
 * Runnable to be scheduled at @ref event_loop using @ref event_loop::schedule_work.
 *
 * All of the work which can be done in the event loop is encapsulated using this class to erase
 * types, so it acts as `std::function` but faster.
 */
class event_loop_work {
  ONLY_DEFAULT_MOVABLE_CLASS(event_loop_work)

private:
  std::coroutine_handle<> handle;

  enum : char { NONE, RESUME, DESTROY } type = NONE;

public:
  /** Creates a instance without any undone work. */
  event_loop_work();

  /** Creates a work which resumes a coroutine handle. */
  event_loop_work(std::coroutine_handle<> h);

  /** Creates a work which destroys a coroutine handle. */
  static event_loop_work create_destroyer(std::coroutine_handle<> h);

  /** Determines if the work is still not done. */
  bool has_work() const;

  /** Does the work. */
  void do_work();
};

/** Mask for socket event types */
enum socket_event_type : char { SOCK_NONE = 0, READABLE = 1, WRITABLE = 2, SOCK_ALL = 3 };

/**
 * Low-level struct representing neccessary data for the event loop to work with a socket.
 *
 * To construct one it is only required to set `fd` and call @ref libev_event_loop::socket_add.
 * However, to receive any updates one should set `event_mask` to something other than `SOCK_NONE`.
 *
 * If the mask of interesting events changes, @ref libev_event_loop::socket_mod should be called.
 *
 * @see        async::socket
 */
struct socket_storage {
  /** Work to execute when socket state changes */
  event_loop_work work;

  /** Pointer to raw event loop's listener for the socket */
  void* event = nullptr;

  /** Socket fd */
  int fd = -1;

  /** Event mask for the event loop to listen to */
  socket_event_type event_mask = SOCK_NONE;

  /** Socket state at last work execution */
  socket_event_type last_event = SOCK_NONE;
};

/**
 * A base class for all of the event loops for @ref async::coro coroutines.
 *
 * One of the implementations of the class should be created and bound to a thread (@ref
 * bind_to_thread) before any usage of the coroutines in the thread (actually before any coroutine
 * suspension). Currently the aforementioned is done in @ref api/src/main.cc.
 *
 * Event loop keeps track of registered event sources and should destroy them in an appropriate
 * moment. The number of alive coroutines is also saved and updated.
 */
class event_loop : public std::enable_shared_from_this<event_loop> {
public:
  /** Pointer to thread's event loop */
  static thread_local std::shared_ptr<event_loop> local;

  /** Number of alive coroutines bound to this loop */
  int alive_coroutines = 0;

  virtual ~event_loop() {}

  /**
   * Starts the event loop.
   *
   * Should be called after @ref bind_to_thread. It is not advisable to try to restart the loop.
   *
   * @param[in]  until_complete  Whether to stop the loop if no coroutines are alive
   *
   * @return     true if there were no unhandled exceptions in top-level coroutines.
   */
  virtual bool run(bool until_complete) = 0;

  /**
   * Notifies the event loop that it should stop.
   *
   * This function is TS, so the intented use case is to ask the loop to stop externally (for
   * example, when stopping a worker running with `until_complete = false` from a main thread).
   */
  virtual void stop_notify() = 0;

  /**
   * Schedules @ref event_loop_work to be run.
   *
   * It is unspecified if the work will be only scheduled or done after function return. @ref
   * libev_event_loop executes the work before returning.
   *
   * @warning    Not TS. One should invent custom ways of scheduling work from the other thread,
   *             for instance, using some event_source.
   *
   * @param[in]  work  Work
   */
  virtual void schedule_work(event_loop_work&& work) = 0;

  /**
   * Handles an uncaught exception from a top-level coroutine.
   *
   * @param[in]  exc   Pointer to the exception
   *
   * @see        detail::on_unhandled_exception_cb
   */
  virtual void handle_exception(std::exception_ptr const& exc) = 0;

  /**
   * Registers new @ref event_source.
   *
   * Everything should be registered before calling @ref bind_to_thread and @ref run.
   *
   * @param[in]  source  Source to register
   */
  virtual void register_source(std::shared_ptr<event_source> source) = 0;

  /**
   * Binds the instance of event loop to the current thread.
   *
   * Consecutive calls to this function overwrite @ref local pointer, so there can only be one
   * active loop in the given execution thread (what makes perfect sense).
   */
  virtual void bind_to_thread() = 0;
};

/**
 * A base class for all of the event sources, which can schedule work in response to external
 * events.
 *
 * Event source is bound to a specific thread alike event_loop, so there is usually one event_source
 * for each source type for each thread.
 *
 * Upon creation a source should be registered using @ref event_loop::register_source. Then the loop
 * will call @ref bind_to_thread, @ref on_init, @ref on_stop which act as callbacks.
 */
class event_source : public std::enable_shared_from_this<event_source> {
public:
  virtual ~event_source() {}

  /** Callback to be run by the event loop when it is being bound to the thread. */
  virtual void bind_to_thread() {}

  /** Callback to be run by the event loop when it is being started. */
  virtual void on_init() {}

  /**
   * Callback to be run by the event loop when it is being shut down.
   *
   * Run in the event loop's thread instead of possibility of some random thread calling
   * destructor.
   */
  virtual void on_stop() {}
};

/** Implementation details */
namespace detail {
  /**
   * Callback for an unhandled exception in a top-level coroutine
   *
   * Actually such exceptions go a long way before being used as an argument here:
   *
   * async::detail::promise_type_facade::unhandled_exception (call generated by compiler) -> @ref
   * async::event_loop::handle_exception (virtual) -> @ref
   * async::detail::log_top_level_exception() -> here.
   *
   * The scheme is required to a) correctly record stacktrace of the exception (in debug builds);
   * b) allow printing external exceptions as if they are exceptions from a top-level coroutine.
   */
  inline std::function<void(std::exception_ptr const&)> on_unhandled_exception_cb;

  /** @see on_unhandled_exception_cb */
  void log_top_level_exception(std::exception_ptr const& e);
}  // namespace detail
}  // namespace async
