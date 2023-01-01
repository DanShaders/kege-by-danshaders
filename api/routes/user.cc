#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "user.pb.h"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"
using async::coro;

namespace {
coro<void> handle_user_login(fcgx::request_t* r) {
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
      q.expect1<int64_t, std::string, std::string, unsigned, std::string, std::string>();
  if (password != utils::b16_encode(utils::sha3_256(req.password() + utils::b16_decode(salt)))) {
    utils::err(r, api::INVALID_CREDENTIALS);
  }

  auto s = std::make_shared<routes::session>(
      user_id, utils::b16_encode(utils::urandom(32)), username, display_name, perms, false,
      routes::session_clock::now().time_since_epoch().count());
  co_await routes::session_store(db, r, s);

  utils::ok<api::UserInfo>(r, s->serialize());
}

coro<void> handle_user_info(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto s = co_await routes::require_auth(db, r);

  utils::ok<api::UserInfo>(r, s->serialize());
}

coro<void> handle_user_logout(fcgx::request_t* r) {
  auto req = utils::expect<api::LogoutRequest>(r);
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto s = co_await routes::require_auth(db, r);

  if (s->user_id != req.user_id()) {
    utils::err(r, api::INVALID_QUERY);
  }
  co_await routes::session_logout(db, s);
  r->headers.push_back("Set-Cookie: kege-session=invalid; expires=Thu, 01 Jan 1970 00:00:00 GMT");
  utils::ok<utils::empty_payload>(r, {});
}

coro<void> handle_check_admin(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await routes::require_auth(db, r, routes::PERM_ADMIN);
}

coro<void> handle_request_id_range(fcgx::request_t* r) {
  const static int64_t MAX_IDS = 20;

  auto try_reserve = [&](auto range) {
    if (range->ptr < range->end) {
      auto result = range->ptr.fetch_add(MAX_IDS);
      if (result < range->end) {
        utils::ok<api::IdRangeResponse>(r, {
                                               .start = result,
                                               .end = std::min(result + MAX_IDS, range->end),
                                           });
        return true;
      }
    }
    return false;
  };

  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto s = co_await routes::require_auth(db, r, routes::PERM_ADMIN);

  int64_t block_len = 2;
  auto where = &s->id_ranges;
  while (auto current = where->load()) {
    if (try_reserve(current)) {
      co_return;
    }
    block_len = 2 * (current->end - current->start);
    where = &current->next;
  }

  auto [end] =
      (co_await db.exec("UPDATE api_id_sequence SET value = value + $1 RETURNING value", block_len))
          .expect1<int64_t>();
  auto start = end - block_len;
  auto owned = std::make_shared<routes::session::owned_id_range>(start, end, start, nullptr);
  where->store(owned);
  if (!try_reserve(owned)) {
    // Extremely sorry for the guy who is calling us concurrently (no).
    utils::err(r, api::EXTREMELY_SORRY);
  }
}
}  //  namespace

ROUTE_REGISTER("/user/info", handle_user_info)
ROUTE_REGISTER("/user/login", handle_user_login)
ROUTE_REGISTER("/user/logout", handle_user_logout)
ROUTE_REGISTER("/user/nginx_auth_only_admins", handle_check_admin)
ROUTE_REGISTER("/user/request-id-range", handle_request_id_range)