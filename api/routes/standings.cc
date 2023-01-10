#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "standings.pb.h"
#include "standings.sql.cc"
#include "utils/api.h"
using async::coro;

namespace {
coro<void> get_html_standings(fcgx::request_t* r) {
  static constexpr int64_t min_id = 0;

  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  int64_t kim_id = utils::expect<int64_t>(r, "id");
  int64_t group_id = utils::expect<int64_t>(r, "gid");

  std::map<int64_t, std::pair<int, int>> tasks;
  int pos = 0;

  r->mime_type = "text/html";
  r->fix_meta();
  r->out << "<table border><thead><tr><td></td><td>Total</td>";

  for (auto [task_id, short_name] : co_await db.exec(GET_KIM_TASKS_REQUEST)) {
    tasks[task_id] = {short_name, pos++};
    r->out << "<td>" << short_name << "</td>";
  }

  r->out << "</tr></thead><tbody>";

  std::map<int64_t, std::vector<double>> ranking;
  std::vector<int64_t> users;
  for (auto [id, task_id, user_id, score, timestamp] : co_await db.exec(GET_USER_ANSWERS_REQUEST)) {
    auto it = ranking.find(user_id);
    if (it == ranking.end()) {
      ranking[user_id] = std::vector<double>(tasks.size());
      users.push_back(user_id);
    }
    ranking[user_id][tasks[task_id].second] = score;
  }

  std::map<int64_t, std::string> usernames;
  for (auto [user_id, username] : co_await db.exec(GET_READABLE_USERNAMES_REQUEST)) {
    usernames[user_id] = std::string(username);
  }

  std::vector<std::tuple<double, std::string_view, std::vector<double>>> sorted;
  for (auto user : users) {
    double result = accumulate(ranking[user].begin(), ranking[user].end(), double(0));
    sorted.emplace_back(result, usernames[user], ranking[user]);
  }
  std::ranges::sort(sorted, std::greater<>{});

  r->out << std::setprecision(2);
  for (auto [score, username, score_by_task] : sorted) {
    r->out << "<tr><td>" << username << "</td><td>" << score << "</td>";
    for (auto curr : score_by_task) {
      r->out << "<td>" << curr << "</td>";
    }
    r->out << "</tr>";
  }
  r->out << "</tbody></table>";
}

coro<void> get_standings(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  auto request = utils::expect<api::StandingsRequest>(r);
  int64_t kim_id = request.kim_id();
  int64_t group_id = request.group_id();

  api::StandingsResponse resp;
  int64_t min_id = request.sync_tag();

  if (!min_id) {
    for (auto [user_id, username] : co_await db.exec(GET_USERS_OF_GROUP_REQUEST)) {
      *resp.add_users() = {{
          .id = user_id,
          .name = std::string(username),
      }};
    }

    for (auto [task_id, short_name] : co_await db.exec(GET_KIM_TASKS_REQUEST)) {
      *resp.add_tasks() = {{
          .id = task_id,
          // FIXME: Better naming of repetitive short_names
          .name = std::to_string(short_name),
      }};
    }
  }

  auto max_id = std::max<int64_t>(1, min_id - 1);
  for (auto [id, task_id, user_id, score, timestamp] : co_await db.exec(GET_USER_ANSWERS_REQUEST)) {
    max_id = std::max(max_id, id);
    *resp.add_submissions() = {{
        .user_id = user_id,
        .task_id = task_id,
        .score = score,
        .timestamp = timestamp,
    }};
  }
  resp.set_sync_tag(max_id + 1);

  utils::ok(r, resp);
}

coro<void> get_user_summary(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);
}

coro<void> get_submission_summary(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  auto request = utils::expect<api::SubmissionSummaryRequest>(r);

  api::SubmissionSummaryResponse resp;
  for (auto [score, timestamp, answer] : co_await db.exec(GET_USER_SUBMISSIONS_REQUEST)) {
    *resp.add_submissions() = {{
        .score = score,
        .timestamp = timestamp,
        .answer = std::string(answer),
    }};
  }

  utils::ok(r, resp);
}

ROUTE_REGISTER("/html-standings", get_html_standings)
ROUTE_REGISTER("/admin/standings", get_standings)
ROUTE_REGISTER("/admin/standings/user-summary", get_user_summary)
ROUTE_REGISTER("/admin/standings/submission-summary", get_submission_summary)
}  // namespace
