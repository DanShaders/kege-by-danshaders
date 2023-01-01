#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "contestant.pb.h"
#include "contestant.sql.cc"
#include "routes.h"
#include "routes/grading.h"
#include "routes/session.h"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"
using async::coro;

namespace {
char const TOKEN_SIGN_KEY[] = {-123, 7,   78, -65, -96, -105, 117, 121,
                               10,   -78, 54, 123, 54,  77,   -97, -58};

struct write_token {
  struct {
    int64_t user_id, kim_id, token_version, start_time, end_time;
  } data;
  static_assert(sizeof(data) == 40);

  char signature[64];

  std::string get_signature() {
    return utils::hmac_sign(std::string_view{reinterpret_cast<char*>(&data), sizeof(data)},
                            std::string_view{TOKEN_SIGN_KEY, sizeof(TOKEN_SIGN_KEY)});
  }

  bool is_valid() {
    return get_signature() == std::string_view{signature, sizeof(signature)};
  }

  void sign() {
    std::ranges::copy(get_signature(), signature);
  }
};
static_assert(sizeof(write_token) == 40 + 64);

struct kim_t {
  int64_t id;
  std::string name;
  int64_t token_version;
  async::pq::timestamp start_time, end_time;
  int64_t duration;
  bool is_virtual;
  bool is_exam;
  std::optional<async::pq::timestamp> virtual_start_time, virtual_end_time;
  api::ParticipationStatus status;
  int64_t user_id;

  api::ContestantKim serialize() const {
    return {{
        .id = id,
        .name = name,
        .start_time = utils::millis_since_epoch(*virtual_start_time),
        .end_time = utils::millis_since_epoch(*virtual_end_time),
        .duration = duration,
        .is_virtual = is_virtual,
        .is_exam = is_exam,
        .status = status,
    }};
  }

  void count_participation_status(async::pq::timestamp current_time) {
    if (current_time < start_time) {
      status = api::NOT_STARTED;
    } else if (start_time <= current_time && current_time < end_time) {
      if (is_virtual) {
        if (!virtual_start_time || !virtual_end_time) {
          status = api::VIRTUAL_STARTABLE;
        } else if (*virtual_start_time <= current_time && current_time < *virtual_end_time) {
          status = api::IN_PROGRESS;
        } else {
          status = api::FINISHED;
        }
      } else {
        status = api::IN_PROGRESS;
      }
    } else {
      status = api::FINISHED;
    }
    if (!virtual_start_time) {
      virtual_start_time = start_time;
    }
    if (!virtual_end_time) {
      virtual_end_time = end_time;
    }
  }

  auto get_write_token() {
    write_token token;
    token.data = {
        .user_id = user_id,
        .kim_id = id,
        .token_version = token_version,
        .start_time = utils::millis_since_epoch(*virtual_start_time),
        .end_time = utils::millis_since_epoch(*virtual_end_time),
    };
    token.sign();
    return token;
  }
};

coro<std::vector<kim_t>> get_available_kims(async::pq::connection& db, int64_t user_id,
                                            async::pq::timestamp current_time) {
  std::vector<kim_t> kims;

  for (auto [id, name, token_version, start_time, end_time, duration, is_virtual, is_exam,
             virtual_start_time, virtual_end_time] : co_await db.exec(AVAILABLE_KIMS_REQUEST)) {
    kim_t kim{
        .id = id,
        .name = name,
        .token_version = token_version,
        .start_time = start_time,
        .end_time = end_time,
        .duration = duration,
        .is_virtual = is_virtual,
        .is_exam = is_exam,
        .virtual_start_time = virtual_start_time,
        .virtual_end_time = virtual_end_time,
        .user_id = user_id,
    };
    kim.count_participation_status(current_time);

    if (kim.status != api::FINISHED && is_exam &&
        start_time - current_time < std::chrono::hours(5)) {
      co_return {kim};
    }
    kims.push_back(std::move(kim));
  }

  co_return kims;
}

coro<void> handle_get_available_kims(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto session = co_await routes::require_auth(db, r);

  api::ContestantKimList list;
  auto current_time = std::chrono::system_clock::now();

  for (auto const& kim : co_await get_available_kims(db, session->user_id, current_time)) {
    *list.add_kims() = kim.serialize();
  }

  utils::ok(r, list);
}

coro<void> handle_get_tasks(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto session = co_await routes::require_auth(db, r);

  auto id = utils::expect<int64_t>(r, "id");
  auto current_time = std::chrono::system_clock::now();

  auto kims = co_await get_available_kims(db, session->user_id, current_time);
  auto kim = std::ranges::find_if(kims, [&](auto const& x) { return x.id == id; });

  if (kim == kims.end()) {
    utils::err(r, api::INVALID_QUERY);
  }

  if (kim->status == api::NOT_STARTED) {
    utils::err(r, api::INVALID_QUERY);
  } else if (kim->status == api::VIRTUAL_STARTABLE) {
    kim->virtual_start_time = current_time;
    kim->virtual_end_time =
        std::min(current_time + std::chrono::milliseconds(kim->duration), kim->end_time);
    co_await db.exec(START_VIRTUAL_REQUEST);
    kim->status = api::IN_PROGRESS;
  }

  auto resp = kim->serialize();

  // Generate write token
  if (kim->status == api::IN_PROGRESS) {
    auto token = kim->get_write_token();
    resp.set_write_token(std::string{reinterpret_cast<char*>(&token), sizeof(token)});
  }

  // Tasks (TODO: add cache)
  std::vector<int64_t> task_ids;
  std::map<int64_t, int> task_position;

  int task_pos = 0;
  for (auto [task_id, task_type, text, answer_rows, answer_cols] :
       co_await db.exec(GET_KIM_TASKS_REQUEST)) {
    task_ids.push_back(task_id);
    task_position[task_id] = task_pos++;

    *resp.add_tasks() = {{
        .id = task_id,
        .task_type = task_type,
        .text = std::string(text),
        .answer_rows = answer_rows,
        .answer_cols = answer_cols,
    }};
  }

  // Attachments
  for (auto [task_id, filename, hash, mime_type] : co_await db.exec(GET_KIM_ATTACHMENTS_REQUEST)) {
    auto task_it = task_position.find(task_id);
    if (task_it == task_position.end()) {
      // KIM was edited during contest
      continue;
    }

    *resp.mutable_tasks(task_it->second)->add_attachments() = {{
        .filename = std::string(filename),
        .mime_type = std::string(mime_type),
        .hash = std::string(hash),
    }};
  }

  // Most recent user answers
  for (auto [task_id, answer] : co_await db.exec(GET_USER_ANSWERS_REQUEST)) {
    auto task_it = task_position.find(task_id);
    if (task_it == task_position.end()) {
      // KIM was edited during contest
      continue;
    }

    resp.mutable_tasks(task_it->second)->set_answer(std::string(answer));
  }

  utils::ok(r, resp);
}

coro<void> handle_answer(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto session = co_await routes::require_auth(db, r);

  auto req = utils::expect<api::ContestantAnswer>(r);

  if (req.write_token().size() != sizeof(write_token)) {
    utils::err(r, api::ACCESS_DENIED);
  }

  write_token token;
  std::ranges::copy(req.write_token(), reinterpret_cast<char*>(&token));

  auto current_time = std::chrono::system_clock::now();

  auto [kim_version] = (co_await db.exec(GET_TOKEN_VERSION_REQUEST)).expect1();

  // Vulnerable to side channel timing attacks, though seems highly unlikely
  if (!token.is_valid() || session->user_id != token.data.user_id) {
    utils::err(r, api::ACCESS_DENIED);
  }

  if (kim_version != token.data.token_version) {
    auto kims = co_await get_available_kims(db, session->user_id, current_time);
    auto kim = std::ranges::find_if(kims, [&](auto const& x) { return x.id == token.data.kim_id; });

    if (kim == kims.end() || kim->status != api::IN_PROGRESS) {
      utils::err(r, api::ACCESS_DENIED);
    }
    token = kim->get_write_token();
  }

  auto current_millis = utils::millis_since_epoch(current_time);

  if (current_millis < token.data.start_time || token.data.end_time <= current_millis) {
    utils::err(r, api::ACCESS_DENIED);
  }

  auto [answer, grading, scale_factor] = (co_await db.exec(GET_REAL_ANSWER_REQUEST)).expect1();
  double score = scale_factor * routes::check_and_grade(req.answer(), answer, grading);

  co_await db.exec(WRITE_ANSWER_REQUEST);

  utils::send_raw(r, api::OK, std::string_view{reinterpret_cast<char*>(&token), sizeof(token)});
}

coro<void> handle_end_request(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto session = co_await routes::require_auth(db, r);

  auto req = utils::expect<api::ParticipationEndRequest>(r);

  co_await db.exec(END_PARTICIPATION_REQUEST);

  utils::ok<utils::empty_payload>(r, {});
}
}  // namespace

ROUTE_REGISTER("/contestant/available-kims", handle_get_available_kims)
ROUTE_REGISTER("/contestant/tasks", handle_get_tasks)
ROUTE_REGISTER("/contestant/answer", handle_answer)
ROUTE_REGISTER("/contestant/end", handle_end_request)
