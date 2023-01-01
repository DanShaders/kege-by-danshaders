#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "standings.sql.cc"
#include "utils/api.h"
using async::coro;

namespace {
coro<void> handle_get_standings(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await routes::require_auth(db, r, routes::PERM_ADMIN);

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
  for (auto [task_id, user_id, score] : co_await db.exec(GET_USER_ANSWERS_REQUEST)) {
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

ROUTE_REGISTER("/html-standings", handle_get_standings)
}  // namespace