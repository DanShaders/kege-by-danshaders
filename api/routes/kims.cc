#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "kims.pb.h"
#include "routes.h"
#include "routes/session.h"
#include "tasks.pb.h"
#include "user.pb.h"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"
using async::coro;

namespace {
coro<void> handle_kim_get_editable(fcgx::request_t *r) {
	auto db = co_await async::pq::connection_pool::local->get_connection();
	co_await routes::require_auth(db, r, routes::PERM_NOT_STUDENT);

	int64_t kim_id = utils::expect<int64_t>(r, "id");

	auto q = co_await db.exec("SELECT * FROM kims WHERE id = $1", kim_id);
	if (!q.rows()) {
		utils::ok<api::Kim>(r, {});
		co_return;
	}

	auto [id, name, start_time, end_time, duration, is_virtual, is_exam, deleted] =
		q.expect1<int64_t, std::string_view, async::pq::timestamp, async::pq::timestamp, int64_t,
				  bool, bool, bool>();

	if (deleted) {
		utils::err(r, api::INVALID_QUERY);
	}

	api::Kim msg{{
		.id = id,
		.name = std::string(name),
		.duration = duration,
		.is_virtual = is_virtual,
		.is_exam = is_exam,
		.start_time = utils::millis_since_epoch(start_time),
		.end_time = utils::millis_since_epoch(end_time),
	}};

	auto q2 = co_await db.exec(
		"SELECT task_id, pos, task_type, tag FROM (kims_tasks JOIN tasks ON tasks.id = "
		"kims_tasks.task_id) WHERE kim_id = $1");
	for (auto [task_id, pos, task_type, tag] : q2.iter<int64_t, int, int64_t, std::string_view>()) {
		*msg.add_tasks() = {{
			.id = task_id,
			.pos = pos,
			.npos = pos,
			.task_type = task_type,
			.tag = std::string(tag),
		}};
	}

	utils::ok(r, msg);
}

coro<void> handle_kim_list(fcgx::request_t *r) {
	auto db = co_await async::pq::connection_pool::local->get_connection();
	co_await routes::require_auth(db, r, routes::PERM_NOT_STUDENT);

	api::KimListResponse msg;

	auto q = co_await db.exec("SELECT * FROM kims WHERE NOT coalesce(deleted, false)");
	for (auto [id, name, start_time, end_time, duration, is_virtual, is_exam, deleted] :
		 q.iter<int64_t, std::string_view, async::pq::timestamp, async::pq::timestamp, int64_t,
				bool, bool, bool>()) {
		*msg.add_kims() = {{
			.id = id,
			.name = std::string(name),
			.duration = duration,
			.is_virtual = is_virtual,
			.is_exam = is_exam,
			.start_time = utils::millis_since_epoch(start_time),
			.end_time = utils::millis_since_epoch(end_time),
		}};
	}

	utils::ok(r, msg);
}
}  // namespace

ROUTE_REGISTER("/kim/$id", handle_kim_get_editable)
ROUTE_REGISTER("/kim/list", handle_kim_list)

// static coro<void> handle_kim_list(fcgx::request_t *r) {
// 	auto db = co_await async::pq::connection_pool::local->get_connection();
// 	auto session = (co_await routes::require_auth(db, r));

// 	api::UserKimListResponse ans{};

// 	auto q2 = co_await db.exec(
// 		"SELECT kims.id, kims.name, kims.duration, kims.start_time, kims.end_time, kims.comment,
// users_kims.state " 		"FROM kims INNER JOIN users_kims ON kims.id IN "
// 		"(SELECT users_kims.kim_id FROM users_kims WHERE users_kims.user_id = $1) "
// 		"AND kims.start_time <= CURRENT_TIMESTAMP AND CURRENT_TIMESTAMP <= kims.end_time "
// 		"AND users_kims.kim_id = kims.id",
// 		user_id);
// 	for (auto [id, name, duration, start_time, end_time, comment, state] :
// 		 q2.iter<int64_t, std::string_view, int, async::pq::timestamp, async::pq::timestamp,
// std::string_view, int>()) { 		logd("{} {} {} {} {} {} {}", id, name, duration, start_time,
// end_time, comment, state); 		*ans.add_kim() = {{ 			.id = id, 			.name =
// std::string(name), .duration = duration, 			.start_time =
// start_time.time_since_epoch().count(), 			.end_time = end_time.time_since_epoch().count(),
// .comment = std::string(comment), 			.state = state,
// 		}};
// 	}

// 	utils::ok(r, ans);
// }

// static coro<void> handle_kim_info(fcgx::request_t *r) {
// 	auto db = co_await async::pq::connection_pool::local->get_connection();
// 	auto user_id = (co_await routes::require_auth(db, r))->user_id;

// 	auto q2 = co_await db.exec(
// 		"SELECT kims.name, kims.duration, kims.start_time, "
// 		"kims.end_time, kims.comment, users_kims.state "
// 		"FROM kims INNER JOIN users_kims ON "
// 		"kims.id = $1 AND users_kims.kim_id = $1 AND users_kims.user_id = $2",
// 		r->params["id"], user_id);
// 	if (!q2.rows()) {
// 		utils::ok<api::Kim>(r, {});
// 		co_return;
// 	}
// 	auto [name, duration, start_time, end_time, state] =
// 		q2.expect1<std::string_view, int64_t, double, double, int>();

// 	api::Kim ans{{
// 		.id = stoi(r->params["id"]),
// 		.name = std::string(name),
// 		.duration = duration,
// 		.start_time = start_time,
// 		.end_time = end_time,
// 		.state = state
// 	}};

// 	utils::ok(r, ans);
// }

// static coro<void> handle_kim_tasks(fcgx::request_t *r) {
// 	auto db = co_await async::pq::connection_pool::local->get_connection();
// 	co_await routes::require_auth(db, r);

// 	api::KimTaskListResponse ans{};

// 	auto q = co_await db.exec(
// 		"SELECT kims_tasks.task_id, kims_tasks.pos FROM kims_tasks WHERE "
// 		"kims_tasks.kim_id = $1",
// 		r->params["id"]);
// 	if (!q.rows()) {
// 		utils::ok<api::KimTaskListResponse>(r, ans);
// 		co_return;
// 	}
// 	for (auto [task_id, pos] : q.iter<int64_t, int>()) {
// 		api::Task task;

// 		auto q1 = co_await db.exec(
// 			"SELECT * FROM tasks WHERE id = $1::bigint AND NOT coalesce(deleted, false)",
// 			r->params["id"]);
// 		if (q1.rows()) {
// 			auto [id, task_type, parent, text, tag, answer_rows, answer_cols, answer, deleted] =
// 				q1.expect1<int64_t, int64_t, int64_t, std::string_view, std::string_view, int, int,
// 						  std::string_view, bool>();

// 			task = api::Task::initializable_type{
// 				.id = id,
// 				.task_type = task_type,
// 				.parent = parent,
// 				.text = std::string(text),
// 				.answer_rows = answer_rows,
// 				.answer_cols = answer_cols,
// 				.answer = std::string(answer),
// 				.tag = std::string(tag),
// 			};

// 			auto q2 = co_await db.exec(
// 				"SELECT id, filename, mime_type, hash, shown_to_user FROM task_attachments WHERE
// task_id
// =
// "
// 				"$1::bigint AND NOT coalesce(deleted, false)",
// 				r->params["id"]);
// 			for (auto [attachment_id, filename, mime_type, hash, shown_to_user] :
// 				 q2.iter<int64_t, std::string_view, std::string_view, std::string_view, bool>()) {
// 				*task.add_attachments() = {{
// 					.id = attachment_id,
// 					.filename = std::string(filename),
// 					.mime_type = std::string(mime_type),
// 					.hash = std::string(hash),
// 					.shown_to_user = shown_to_user,
// 				}};
// 			}
// 		}

// 		*ans.add_tasks() = {{
// 			.task = task,
// 			.pos = pos
// 		}};
// 	}

// 	utils::ok(r, ans);
// }

// static coro<void> handle_kim_user_answers(fcgx::request_t *r) {
// 	auto db = co_await async::pq::connection_pool::local->get_connection();
// 	co_await routes::require_auth(db, r);

// 	auto cookies_it = r->cookies.find("kege-session");
// 	const auto &session_id = cookies_it->second;

// 	auto q1 = co_await db.exec(
// 		"SELECT users.id "
// 		"FROM (sessions JOIN users ON sessions.user_id = users.id) WHERE sessions.id = $1",
// 		session_id);
// 	if (q1.rows() != 1) {
// 		utils::err(r, api::ACCESS_DENIED);
// 	}
// 	auto [user_id] = q1.expect1<int>();

// 	api::KimUserAnswerListResponse ans{};

// 	auto q2 = co_await db.exec(
// 		"SELECT users_answers.pos, users_answers.answer, users_answers.submit_time "
// 		"FROM kims_tasks WHERE users_answers.kim_id = $1 AND users_answers.user_id = $2",
// 		r->params["id"], user_id);
// 	if (!q2.rows()) {
// 		utils::ok<api::KimUserAnswerListResponse>(r, ans);
// 		co_return;
// 	}
// 	for (auto [pos, answer, submit_time] :
// 		 q2.iter<int, std::string_view, int64_t>()) {
// 		*ans.add_answers() = {{
// 			.pos = pos,
// 			.answer = std::string(answer),
// 			.submit_time = submit_time
// 		}};
// 	}

// 	utils::ok(r, ans);
// }

// static coro<void> handle_update_state(fcgx::request_t *r) {
// 	auto db = co_await async::pq::connection_pool::local->get_connection();
// 	co_await routes::require_auth(db, r);

// 	auto cookies_it = r->cookies.find("kege-session");
// 	const auto &session_id = cookies_it->second;

// 	auto q1 = co_await db.exec(
// 		"SELECT users.id "
// 		"FROM (sessions JOIN users ON sessions.user_id = users.id) WHERE sessions.id = $1",
// 		session_id);
// 	if (q1.rows() != 1) {
// 		utils::err(r, api::ACCESS_DENIED);
// 	}
// 	auto [user_id] = q1.expect1<int>();

// 	co_await db.transaction();

// 	auto q2 = co_await db.exec(
// 		"UPDATE users_kims SET state = $2 "
// 		"WHERE users_kims.kim_id = $1 AND users_kims.user_id = $3",
// 		r->params["id"], r->params["state"], user_id);

// 	co_await db.commit();

// 	utils::ok(r, utils::empty_payload{});
// }

// ROUTE_REGISTER("/kim/list", handle_kim_list)
// ROUTE_REGISTER("/kim/info/$id", handle_kim_info)
// ROUTE_REGISTER("/kim/tasks/$id", handle_kim_tasks)
// ROUTE_REGISTER("/kim/user/answers/$id", handle_kim_user_answers)
// ROUTE_REGISTER("/kim/update/state/$id/$state", handle_update_state)
