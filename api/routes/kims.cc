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
		"SELECT task_id, task_type, tag FROM (kims_tasks JOIN tasks ON tasks.id = "
		"kims_tasks.task_id) WHERE kim_id = $1 ORDER BY pos",
		kim_id);
	int task_index = 0;
	for (auto [task_id, task_type, tag] : q2.iter<int64_t, int64_t, std::string_view>()) {
		*msg.add_tasks() = {{
			.id = task_id,
			.curr_pos = task_index,
			.swap_pos = task_index,
			.task_type = task_type,
			.tag = std::string(tag),
		}};
		// pos from database is not required to be consecutive
		++task_index;
	}

	utils::ok(r, msg);
}

// clang-format off
const char KIM_UPDATE_SQL[] =
	"INSERT INTO kims ("
	  "id, name, start_time, end_time, "
	  "duration, virtual, exam"
	") "
	"VALUES "
	  "("
	    "$1, "
	    "(CASE WHEN $3 THEN $2::text ELSE NULL END), "
	    "(CASE WHEN $5 THEN $4::timestamp ELSE NULL END), "
	    "(CASE WHEN $7 THEN $6::timestamp ELSE NULL END), "
	    "(CASE WHEN $9 THEN $8::bigint ELSE NULL END), "
	    "(CASE WHEN $11 THEN $10::bool ELSE NULL END), "
	    "(CASE WHEN $13 THEN $12::bool ELSE NULL END)"
	  ")"
	"ON CONFLICT (id) DO UPDATE SET "
	  "name = (CASE WHEN $3 THEN excluded ELSE kims END).name, "
	  "start_time = (CASE WHEN $5 THEN excluded ELSE kims END).start_time, "
	  "end_time = (CASE WHEN $7 THEN excluded ELSE kims END).end_time, "
	  "duration = (CASE WHEN $9 THEN excluded ELSE kims END).duration, "
	  "virtual = (CASE WHEN $11 THEN excluded ELSE kims END).virtual, "
	  "exam = (CASE WHEN $13 THEN excluded ELSE kims END).exam "
	"RETURNING (xmax = 0)";
// clang-format on

coro<void> handle_kim_update(fcgx::request_t *r) {
	auto db = co_await async::pq::connection_pool::local->get_connection();
	auto session = co_await routes::require_auth(db, r, routes::PERM_NOT_STUDENT);

	auto kim = utils::expect<api::Kim>(r);
	if (!kim.id()) {
		utils::err(r, api::INVALID_QUERY);
	}

	// Protect from overflows to be on the save side.
	auto check_interval = [r](int64_t what, int64_t lo, int64_t hi) {
		if (what < lo || what > hi) {
			utils::err(r, api::INVALID_QUERY);
		}
	};
	// 5e12ms as ns can fit into int64_t.
	check_interval(kim.start_time(), -5e12, 5e12);
	check_interval(kim.end_time(), -5e12, 5e12);
	check_interval(kim.duration(), -5e12, 5e12);

	co_await db.transaction();

	auto start_time = async::pq::timestamp(std::chrono::milliseconds(kim.start_time())),
		 end_time = async::pq::timestamp(std::chrono::milliseconds(kim.end_time()));
	auto [is_kim_inserted] =
		(co_await db.exec(KIM_UPDATE_SQL, kim.id(), kim.name(), kim.has_name(), start_time,
						  kim.has_start_time(), end_time, kim.has_end_time(), kim.duration(),
						  kim.has_duration(), kim.is_virtual(), kim.has_is_virtual(), kim.is_exam(),
						  kim.has_is_exam()))
			.expect1<bool>();

	if (is_kim_inserted && !is_id_owned_by(session, kim.id())) {
		utils::err(r, api::EXTREMELY_SORRY);
	}

	// Process task swaps
	// 1) We want mapping user_pos -> db_pos.
	std::vector<int64_t> task_ids;
	for (auto task : kim.tasks()) {
		if (task.curr_pos() != -1) {
			task_ids.push_back(task.id());
		}
	}

	auto q =
		(co_await db.exec("SELECT task_id, pos FROM unnest($1::bigint[]) AS ids JOIN kims_tasks ON "
						  "task_id = ids WHERE kim_id = $2 ORDER BY (task_id, pos) FOR UPDATE",
						  task_ids, kim.id()));
	std::map<int64_t, int> db_positions;
	for (auto [key, value] : q.iter<int64_t, int>()) {
		db_positions[key] = value;
	}

	std::map<int, int> user_pos_remap;
	for (auto task : kim.tasks()) {
		if (task.curr_pos() != -1) {
			user_pos_remap[task.curr_pos()] = db_positions[task.id()];
		}
	}

	// 2) There might be some new "introduced" positions, they only appear in swap_pos.
	std::vector<int> introduced_pos;
	for (auto task : kim.tasks()) {
		if (auto pos = task.swap_pos(); pos != -1 && !user_pos_remap.count(pos)) {
			introduced_pos.push_back(pos);
		}
	}
	std::ranges::sort(introduced_pos);

	// 3) Reserve "introduced" positions in db
	int new_start_pos = -1, new_end_pos = -1;
	int introduced_cnt = static_cast<int>(introduced_pos.size());

	if (is_kim_inserted) {
		new_start_pos = 0, new_end_pos = introduced_cnt;
		co_await db.exec("INSERT INTO kims_tasks ($1, NULL, $2)", kim.id(), new_end_pos);
	}
	if (introduced_cnt && !is_kim_inserted) {
		std::tie(new_end_pos) =
			(co_await db.exec("UPDATE kims_tasks SET pos = pos + $1 WHERE kim_id = "
							  "$2 AND task_id IS NULL RETURNING pos",
							  introduced_cnt, kim.id()))
				.expect1<int>();
		new_start_pos = new_end_pos - introduced_cnt;
	}
	for (int pos : introduced_pos) {
		user_pos_remap[pos] = new_start_pos++;
	}

	// 4) Delete what needs to be deleted
	std::vector<int64_t> delete_ids, swap_ids;
	std::vector<int> swap_positions;
	for (auto task : kim.tasks()) {
		if (task.swap_pos() == -1) {
			if (task.curr_pos() == -1) {
				// nice trolling
				continue;
			}
			delete_ids.push_back(task.id());
		} else {
			swap_ids.push_back(task.id());
			swap_positions.push_back(user_pos_remap[task.swap_pos()]);
		}
	}
	if (delete_ids.size()) {
		co_await db.exec(
			"DELETE FROM kims_tasks USING unnest($1::bigint[]) AS ids WHERE kim_id = $2 AND "
			"task_id = ids",
			delete_ids, kim.id());
	}

	// 5) Finally swap
	if (swap_ids.size()) {
		co_await db.exec(
			"INSERT INTO kims_tasks VALUES ($1, unnest($2::bigint[]), unnest($3::int[])) ON "
			"CONFLICT"
			" (kim_id, task_id) DO UPDATE SET pos = excluded.pos",
			kim.id(), swap_ids, swap_positions);
	}
	co_await db.commit();

	utils::ok(r, utils::empty_payload{});
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
ROUTE_REGISTER("/kim/update", handle_kim_update)
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
