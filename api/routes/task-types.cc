#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "task-types.pb.h"
#include "utils/api.h"
using async::coro;

static coro<void> handle_list(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await routes::require_auth(db, r);

  api::TaskTypeListResponse ans{};

  auto q = co_await db.exec("SELECT * FROM task_types WHERE NOT deleted ORDER BY short_name");
  for (auto [id, obsolete, short_name, full_name, grading, scale_factor, deleted] :
       q.iter<int64_t, bool, int, std::string_view, int, double, bool>()) {
    *ans.add_type() = api::TaskType::initializable_type{
        .id = id,
        .obsolete = obsolete,
        .short_name = std::to_string(short_name),
        .full_name = std::string(full_name),
        .grading = (api::GradingPolicy) grading,
        .scale_factor = scale_factor,
        .deleted = false,
    };
  }
  utils::ok(r, ans);
}

ROUTE_REGISTER("/task-types/list", handle_list)
