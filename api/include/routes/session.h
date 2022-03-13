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
	const int user_id;
	const std::string session_id;
	const std::string username;
	const std::string display_name;
	const unsigned perms;
	std::atomic<int64_t> last_activity;

	api::UserInfo::initializable_type serialize() const;
};

using psession = std::shared_ptr<session>;

enum {
	PERM_NOT_STUDENT = 1,     // view admin interface
	PERM_VIEW_TASKS = 2,      // view all kims & tasks with answers
	PERM_VIEW_STANDINGS = 4,  // view standings
	PERM_WRITE_TASKS = 8,     // modify kims & tasks
	PERM_MANAGE_USERS = 16    // manage users & groups
};

coro<psession> _restore_session(const async::pq::connection &db, fcgx::request_t *r);
coro<psession> require_auth(const async::pq::connection &db, fcgx::request_t *r,
							unsigned perms = 0);
coro<void> session_store(const async::pq::connection &db, fcgx::request_t *r, psession s);
coro<void> session_logout(const async::pq::connection &db, psession s);
}  // namespace routes