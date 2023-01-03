#pragma once

#include "api.pb.h"
#include "async/coro.h"
#include "fcgx.h"

namespace routes {
inline constexpr auto session_logout_afk_time = std::chrono::hours(5);

// Session might be referenced by multiple shared pointers in different workers, hence different
// threads at the same time. We do not have locks here, so all of the fields should be either const
// or atomic.
struct session {
  int64_t const user_id;
  std::string const username;
  std::string const display_name;
  unsigned const perms;

  std::atomic<bool> logged_out = false;
  std::atomic<int64_t> last_activity = 0;

  struct owned_id_range {
    const int64_t start, end;
    std::atomic<int64_t> ptr;
    // FIXME: Do not use atomic<shared_ptr>
    std::atomic<std::shared_ptr<owned_id_range>> next;
  };
  std::atomic<std::shared_ptr<owned_id_range>> id_ranges;

  bool is_owner_of(int64_t object_id) const;
  api::UserInfo::initializable_type serialize() const;
  static coro<void> save(std::shared_ptr<session> s, std::string_view session_id);
};

enum class Permission {
  NONE = 0,
  ADMIN = 1,
};

coro<std::shared_ptr<session>> require_auth(fcgx::request_t* r, Permission mask);
}  // namespace routes
