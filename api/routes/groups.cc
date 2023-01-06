#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "groups.pb.h"
#include "groups.sql.cc"
#include "routes.h"
#include "routes/session.h"
#include "utils/api.h"

using async::coro;

namespace {
coro<void> get_group_list(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  api::GroupListResponse response;
  for (auto [id, name] : co_await db.exec(GET_GROUPS_LIST_REQUEST)) {
    *response.add_groups() = api::Group::initializable_type{
        .id = id,
        .name = name,
    };
  }

  utils::ok(r, response);
}
}  // namespace

ROUTE_REGISTER("/groups/list", get_group_list)
