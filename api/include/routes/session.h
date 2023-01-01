#pragma once

#include "api.pb.h"
#include "async/coro.h"
#include "async/pq.h"
#include "fcgx.h"

namespace routes {
using session_clock = std::chrono::system_clock;

inline session_clock::duration LOGOUT_TIME = std::chrono::hours(5);

// Session might be referenced by multiple shared pointers in different workers, hence different
// threads at the same time. We do not have locks here, so all of the fields should be either const
// or atomic.
struct session {
  const int64_t user_id;
  const std::string session_id;
  const std::string username;
  const std::string display_name;
  unsigned const perms;
  std::atomic<bool> logged_out;
  std::atomic<int64_t> last_activity;

  struct owned_id_range {
    const int64_t start, end;
    std::atomic<int64_t> ptr;
    std::atomic<std::shared_ptr<owned_id_range>> next;
  };
  std::atomic<std::shared_ptr<owned_id_range>> id_ranges;

  api::UserInfo::initializable_type serialize() const;
};

using psession = std::shared_ptr<session>;

enum {
  PERM_ADMIN = 1,     // view admin interface
};

coro<psession> _restore_session(async::pq::connection const& db, fcgx::request_t* r);
coro<psession> require_auth(async::pq::connection const& db, fcgx::request_t* r,
                            unsigned perms = 0);
coro<void> session_store(async::pq::connection const& db, fcgx::request_t* r, psession s);
coro<void> session_logout(async::pq::connection const& db, psession s);

bool is_id_owned_by(psession s, int64_t id);
}  // namespace routes