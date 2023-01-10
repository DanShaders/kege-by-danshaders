#pragma once

#include "stdafx.h"

#include "event-loop.h"

namespace async {
class mutex;
class lock_guard;

/**
 * Asynchronous, non-recursive, not thread-safe mutex
 */
class mutex {
  DEFAULT_MOVABLE_CLASS(mutex)
  DEFAULT_COPIABLE_CLASS(mutex)

  friend lock_guard;

private:
  struct state_t;

  std::shared_ptr<state_t> state;

public:
  /** @private */
  class locker {
    IMMOVABLE_CLASS(locker)
    UNCOPIABLE_CLASS(locker)

    friend mutex;

  protected:
    std::shared_ptr<state_t> state;

    locker(std::shared_ptr<state_t> state_);

  public:
    bool await_ready() noexcept;
    void await_resume();
    void await_suspend(std::coroutine_handle<> h) noexcept;
  };

  mutex();

  /**
   * Locks the mutex. Awaiting the result of this call may throw an exception if @ref cancel_all
   * is called meanwhile.
   *
   * @return     Awaitable which locks the mutex when completed.
   */
  locker lock();

  /**
   * Locks the mutex if it is in an unlocked state. Returns immediately.
   *
   * @return     Whether the operation was successful.
   */
  bool try_lock();

  /**
   * Unlocks the mutex.
   *
   * You are free to <s>shoot at your leg</s> unlock a mutex currently held by another coroutine.
   */
  void unlock() noexcept;

  /**
   * Cancels all awaiting lock requests on this mutex by finishing them with a given exception.
   *
   * @param[in]  exc   Exception to throw.
   */
  void cancel_all(std::exception_ptr const& exc) noexcept;
};

/**
 * Helper to automatically call @ref async::mutex::unlock on scope exit.
 */
class lock_guard : public mutex::locker {
public:
  /**
   * Constructs lock_guard. Unlike std::lock_guard, it is a caller responsibility to lock the
   * mutex.
   *
   * @param[in]  m     Mutex to unlock on scope exit.
   */
  lock_guard(mutex const& m);

  ~lock_guard();
};
}  // namespace async
