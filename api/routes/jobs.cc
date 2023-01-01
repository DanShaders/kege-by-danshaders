#include "stdafx.h"

#include <fmt/format.h>

#include "async/coro.h"
#include "async/pq.h"
#include "jobs.pb.h"
#include "routes.h"
#include "routes/jobs.h"
#include "routes/session.h"
#include "utils/api.h"
using async::coro;

static coro<void> handle_list(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await routes::require_auth(db, r, routes::PERM_NOT_STUDENT);

  auto q = co_await db.exec("SELECT id, type, status FROM jobs");

  api::Jobs result;
  for (auto [id, type, status] : q.iter<int64_t, int, std::string_view>()) {
    if (type == routes::job_file_import::DB_TYPE && status.size() > 1) {
      using namespace routes::job_file_import;

      *result.add_jobs() = api::Jobs_Desc::initializable_type{
          .id = id,
          .desc = fmt::format("Импорт заданий из файла {}", status.substr(1)),
          .status = STATUS[std::clamp<int>(status[0], 0, STATUS_LEN - 1)],
          .open_link = "admin/job-file-import",
      };
    }
  }
  utils::ok(r, result);
}

ROUTE_REGISTER("/jobs/list", handle_list)
