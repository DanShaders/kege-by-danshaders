#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "kims.pb.h"
#include "kims.sql.cc"
#include "routes.h"
#include "routes/grading.h"
#include "routes/session.h"
#include "utils/api.h"
#include "utils/common.h"
using async::coro;

namespace {
coro<void> handle_kim_get_editable(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  int64_t kim_id = utils::expect<int64_t>(r, "id");

  auto q = co_await db.exec(GET_KIM_REQUEST);
  if (!q.rows()) {
    utils::ok<api::Kim>(r, {});
    co_return;
  }

  auto [id, name, token_version, deleted] = q.expect1();
  if (deleted) {
    utils::err(r, api::INVALID_QUERY);
  }

  api::Kim msg{{
      .id = id,
      .name = std::string(name),
  }};

  int task_index = 0;
  for (auto [task_id, task_type, tag] : co_await db.exec(GET_KIM_TASKS_REQUEST)) {
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

  for (auto [group_id, kim_id_, start_time, end_time, duration, is_virtual, is_exam] :
       co_await db.exec(GET_KIM_GROUPS_REQUEST)) {
    *msg.add_groups() = {{
        .id = group_id,
        .start_time = utils::millis_since_epoch(start_time),
        .end_time = utils::millis_since_epoch(end_time),
        .duration = duration,
        .is_virtual = is_virtual,
        .is_exam = is_exam,
    }};
  }

  utils::ok(r, msg);
}

coro<void> handle_kim_update(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto session = co_await require_auth(r, routes::Permission::ADMIN);

  auto kim = utils::expect<api::Kim>(r);
  if (!kim.id()) {
    utils::err(r, api::INVALID_QUERY);
  }

  co_await db.transaction();

  // Generic KIM info
  auto [is_kim_inserted] = (co_await db.exec(INSERT_OR_UPDATE_KIM_REQUEST)).expect1();

  if (is_kim_inserted && !session->is_owner_of(kim.id())) {
    utils::err(r, api::EXTREMELY_SORRY);
  }

  // Task list modifications
  {
    // 1) We want mapping user_pos -> db_pos.
    std::vector<int64_t> task_ids;
    for (auto task : kim.tasks()) {
      if (task.curr_pos() != -1) {
        task_ids.push_back(task.id());
      }
    }

    std::map<int64_t, int> db_positions;
    for (auto [key, value] : co_await db.exec(LOCK_MENTIONED_TASKS_REQUEST)) {
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
      co_await db.exec(INSERT_TASK_COUNT_RECORD_REQUEST);
    }
    if (introduced_cnt && !is_kim_inserted) {
      std::tie(new_end_pos) = (co_await db.exec(UPDATE_TASK_COUNT_RECORD_REQUEST)).expect1();
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
      co_await db.exec(DELETE_TASKS_REQUEST);
    }

    // 5) Finally swap
    if (swap_ids.size()) {
      co_await db.exec(INSERT_AND_SWAP_TASKS_REQUEST);
    }
  }

  // Assignment modification
  auto check_interval = [r](int64_t what, int64_t lo, int64_t hi) {
    if (what < lo || what > hi) {
      utils::err(r, api::INVALID_QUERY);
    }
  };

  for (auto entry : kim.groups()) {
    if (entry.deleted()) {
      co_await db.exec(DELETE_GROUP_ASSIGNMENT_REQUEST);
    } else {
      // 5e12ms as ns can fit into int64_t.
      check_interval(entry.start_time(), -5e12, 5e12);
      check_interval(entry.end_time(), -5e12, 5e12);
      check_interval(entry.duration(), 0, 5e12);

      auto start_time = async::pq::timestamp(std::chrono::milliseconds(entry.start_time())),
           end_time = async::pq::timestamp(std::chrono::milliseconds(entry.end_time()));

      auto [is_sane] = (co_await db.exec(INSERT_OR_MODIFY_GROUP_ASSIGNMENTS_REQUEST)).expect1();
      if (!is_sane) {
        utils::err(r, api::INVALID_QUERY);
      }
    }
  }

  co_await db.commit();
  utils::ok(r, utils::empty_payload{});
}

coro<void> handle_kim_list(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  api::KimListResponse msg;

  for (auto [id, name, token_version, deleted] : co_await db.exec(GET_KIM_LIST_REQUEST)) {
    *msg.add_kims() = {{
        .id = id,
        .name = std::string(name),
    }};
  }

  utils::ok(r, msg);
}

coro<void> revoke_access_keys(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);
  co_await db.exec(BUMP_KIM_VERSION_REQUEST);
  utils::ok(r, utils::empty_payload{});
}

coro<void> rejudge_submissions(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  int64_t kim_id = utils::expect<int64_t>(r, "id");

  for (auto [task_id, task_type, jury_answer, grading, scale_factor] :
       co_await db.exec(COLLECT_TASKS_FROM_KIM_REQUEST)) {
    std::vector<int64_t> ids;
    std::vector<double> scores;

    for (auto [id, user_answer] : co_await db.exec(COLLECT_USER_ANSWERS_REQUEST)) {
      double score = scale_factor * routes::check_and_grade(user_answer, jury_answer, grading);

      ids.push_back(id);
      scores.push_back(score);
    }

    co_await db.exec(WRITE_REJUDGED_SCORES_REQUEST);
  }

  utils::ok(r, utils::empty_payload{});
}

coro<void> delete_kim(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  auto req = utils::expect<api::KimDeleteRequest>(r);
  co_await db.exec(DELETE_KIM_REQUEST);

  utils::ok(r, utils::empty_payload{});
}

coro<void> clone_answers(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  auto req = utils::expect<api::CloneAnswersRequest>(r);
  co_await db.exec(CLONE_ANSWERS_REQUEST);

  utils::ok(r, utils::empty_payload{});
}
}  // namespace

ROUTE_REGISTER("/kim/$id", handle_kim_get_editable)
ROUTE_REGISTER("/kim/update", handle_kim_update)
ROUTE_REGISTER("/kim/list", handle_kim_list)
ROUTE_REGISTER("/kim/$id/revoke-access-keys", revoke_access_keys)
ROUTE_REGISTER("/kim/$id/rejudge-submissions", rejudge_submissions)
ROUTE_REGISTER("/kim/delete", delete_kim)
ROUTE_REGISTER("/kim/clone-answers", clone_answers)
