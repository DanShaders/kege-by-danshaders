#include "stdafx.h"

#include "KEGE.h"
#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "tasks.pb.h"
#include "utils/api.h"
using async::coro;

static coro<void> handle_get(fcgx::request_t *r) {
	auto db = co_await async::pq::connection_pool::local->get_connection();
	co_await routes::require_auth(db, r, routes::PERM_NOT_STUDENT);

	auto q = co_await db.exec("SELECT * FROM tasks WHERE id = $1::bigint AND NOT deleted",
							  r->params["id"]);
	auto [id, task_type, parent, task, answer_rows, answer_cols, answer, deleted] =
		q.expect1<int64_t, int64_t, int64_t, std::string_view, int, int, std::string_view, bool>();

	utils::ok<api::Task>(r, {
		.id = id,
		.task_type = task_type,
		.parent = parent,
		.text = std::string(task),
		.answer_rows = answer_rows,
		.answer_cols = answer_cols,
		.answer = std::string(answer),
	});
}

ROUTE_REGISTER("/tasks/get", handle_get)
