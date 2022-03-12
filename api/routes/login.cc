#include "stdafx.h"

#include "KEGE.h"
#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"
using async::coro;

static coro<void> handle_login(fcgx::request_t *r) {
	auto req = utils::expect<api::LoginRequest>(r);
	auto db = co_await async::pq::connection_pool::local->get_connection();

	auto q = co_await db.exec(
		"SELECT id, username, display_name, permissions, salt, password FROM users WHERE username "
		"= $1",
		req.username());
	if (q.rows() != 1) {
		utils::err(r, api::INVALID_CREDENTIALS);
	}
	auto [user_id, username, display_name, perms, salt, password] =
		q.expect1<int, std::string, std::string, unsigned, std::string, std::string>();
	if (password != utils::b16_encode(utils::sha3_256(req.password() + utils::b16_decode(salt)))) {
		utils::err(r, api::INVALID_CREDENTIALS);
	}

	auto session_obj = new routes::session{
		.user_id = user_id,
		.session_id = utils::b16_encode(utils::urandom(32)),
		.username = username,
		.display_name = display_name,
		.perms = perms,
		.last_activity = routes::session_clock::now().time_since_epoch().count()};
	routes::session_store(db, r, routes::psession(session_obj));

	utils::ok<api::UserInfo>(
		r,
		{.user_id = user_id, .username = username, .display_name = display_name, .perms = perms});
	co_return;
}

ROUTE_REGISTER("/login", handle_login)
