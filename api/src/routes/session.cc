#include "routes/session.h"

#include "utils/api.h"
using namespace routes;

namespace {
std::mutex cache_lock;
std::map<std::string_view, psession, std::less<>> cache;

void add_to_cache(psession s) {
	auto lock = std::lock_guard{cache_lock};
	cache[s->session_id] = s;
}
}  // namespace

coro<psession> routes::_restore_session(const async::pq::connection &db, fcgx::request_t *r) {}

coro<psession> routes::require_auth(const async::pq::connection &db, fcgx::request_t *r,
									int perms) {
	auto s = co_await _restore_session(db, r);
	if (!s || (s->perms & perms) != perms) {
		utils::err(r, api::ACCESS_DENIED);
	}
}

coro<void> routes::session_store(const async::pq::connection &db, fcgx::request_t *r, psession s) {
	using namespace std::literals;

	r->headers.push_back("Set-Cookie: kege-session="s + s->session_id + "; Secure; HttpOnly");
	co_await db.exec("INSERT INTO sessions VALUES ($1, $2, $3)", s->session_id, s->user_id,
					 session_clock::now().time_since_epoch().count());
	co_await db.exec(
		"UPDATE users SET last_login_time = CURRENT_TIMESTAMP(), last_login_method = 'ip=' + $2, "
		"WHERE id = $1",
		s->user_id, r->remote_ip);
	{
		auto lock = std::lock_guard{cache_lock};
		cache[s->session_id] = s;
	}
}

coro<void> routes::session_logout(const async::pq::connection &db, psession s) {
	{
		auto lock = std::lock_guard{cache_lock};
		cache[s->session_id] = {};
	}
	co_await db.exec("DELETE FROM sessions WHERE id = $1", s->session_id);
	{
		auto lock = std::lock_guard{cache_lock};
		cache.erase(s->session_id);
	}
}
