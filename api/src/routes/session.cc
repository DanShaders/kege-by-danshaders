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

api::UserInfo::initializable_type session::serialize() const {
	return {.user_id = user_id, .username = username, .display_name = display_name, .perms = perms};
}

coro<psession> routes::_restore_session(const async::pq::connection &db, fcgx::request_t *r) {
	auto cookies_it = r->cookies.find("kege-session");
	if (cookies_it == r->cookies.end()) {
		co_return {};
	}
	const auto &session_id = cookies_it->second;

	{
		auto lock = std::lock_guard{cache_lock};
		auto cache_it = cache.find(session_id);
		if (cache_it != cache.end()) {
			co_return cache_it->second->logged_out ? psession{} : cache_it->second;
		}
	}

	auto q = co_await db.exec(
		"SELECT users.id, username, display_name, permissions "
		"FROM (sessions JOIN users ON sessions.user_id = users.id) WHERE sessions.id = $1",
		session_id);
	if (q.rows() != 1) {
		utils::err(r, api::ACCESS_DENIED);
	}
	auto [user_id, username, display_name, perms] =
		q.expect1<int64_t, std::string, std::string, unsigned>();

	auto s =
		std::make_shared<routes::session>(user_id, session_id, username, display_name, perms, false,
										  routes::session_clock::now().time_since_epoch().count());
	add_to_cache(s);
	co_return s;
}

coro<psession> routes::require_auth(const async::pq::connection &db, fcgx::request_t *r,
									unsigned perms) {
	auto s = co_await _restore_session(db, r);
	if (!s || (s->perms & perms) != perms) {
		utils::err(r, api::ACCESS_DENIED);
	}
	co_return s;
}

coro<void> routes::session_store(const async::pq::connection &db, fcgx::request_t *r, psession s) {
	using namespace std::literals;

	r->headers.push_back("Set-Cookie: kege-session="s + s->session_id +
						 "; Path=/; Max-Age=86400; Secure; HttpOnly");
	co_await db.exec("INSERT INTO sessions VALUES ($1, $2, $3)", s->session_id, s->user_id,
					 session_clock::now().time_since_epoch().count());
	co_await db.exec(
		"UPDATE users SET last_login_time = CURRENT_TIMESTAMP, last_login_method = CONCAT('ip=', "
		"$2::text) WHERE id = $1",
		s->user_id, r->remote_ip);
	add_to_cache(s);
}

coro<void> routes::session_logout(const async::pq::connection &db, psession s) {
	if (s->logged_out.exchange(true)) {
		// User is requesting logouts concurrently
		co_return;
	}

	co_await db.exec("DELETE FROM sessions WHERE id = $1", s->session_id);
	{
		auto lock = std::lock_guard{cache_lock};
		cache.erase(s->session_id);
	}
}

bool routes::is_id_owned_by(psession s, int64_t id) {
	auto current_range = s->id_ranges.load();
	while (current_range) {
		if (current_range->start <= id && id < current_range->ptr && id < current_range->end) {
			return true;
		}
		current_range = current_range->next.load();
	}
	return false;
}
