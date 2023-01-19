#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "users.pb.h"
#include "users.sql.cc"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"

using async::coro;

namespace {
coro<void> list_users_as_plain_text(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  r->mime_type = "text/plain;charset=UTF-8";
  r->fix_meta();

  std::map<int64_t, std::string> groups;
  for (auto [group_id, display_name] : co_await db.exec(LIST_GROUPS_REQUEST)) {
    groups[group_id] = std::string(display_name);
  }

  for (auto [id, username, display_name, permissions] : co_await db.exec(LIST_ALL_USERS_REQUEST)) {
    r->out << fmt::format("id={} username={} display_name={} perms={}\n", id, username,
                          display_name, permissions);
  }

  int64_t prev_group = -1;
  for (auto [user_id, group_id] : co_await db.exec(LIST_GROUP_MAPPING_REQUEST)) {
    if (prev_group != group_id) {
      prev_group = group_id;
      r->out << fmt::format("\ngroup id={} name={}:\n", group_id, groups[group_id]);
    }
    r->out << user_id << " ";
  }
}

coro<void> replace_users(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await require_auth(r, routes::Permission::ADMIN);

  co_await db.transaction();
  co_await db.exec(DROP_ALL_USERS_REQUEST);
  co_await db.exec(DROP_ALL_GROUPS_REQUEST);

  auto list = utils::expect<api::UserReplaceRequest>(r);
  size_t user_count = static_cast<size_t>(list.users_size());

  std::string passwords_owner;
  std::vector<std::string_view> usernames, display_names, salts, passwords;

  for (size_t i = 0; i < user_count; ++i) {
    auto& user = list.users(static_cast<int>(i));
    std::string salt = utils::urandom(32);
    std::string password = utils::sha3_256(user.password() + salt);

    usernames.push_back(user.username());
    display_names.push_back(user.display_name());
    passwords_owner += salt;
    passwords_owner += password;
  }

  passwords_owner = utils::b16_encode(passwords_owner);
  for (size_t i = 0; i < user_count; ++i) {
    salts.push_back(std::string_view(passwords_owner).substr(128 * i, 64));
    passwords.push_back(std::string_view(passwords_owner).substr(128 * i + 64, 64));
  }

  std::vector<int64_t> user_ids;
  for (auto [id] : co_await db.exec(ADD_USERS_REQUEST)) {
    user_ids.push_back(id);
  }

  std::vector<std::string_view> group_names;
  for (int i = 0; i < list.groups_size(); ++i) {
    group_names.push_back(list.groups(i));
  }
  std::vector<int64_t> group_ids;
  for (auto [id] : co_await db.exec(ADD_GROUPS_REQUEST)) {
    group_ids.push_back(id);
  }

  std::vector<int64_t> map_user_id, map_group_id;
  for (size_t i = 0; i < user_count; ++i) {
    for (auto group : list.users(static_cast<int>(i)).groups()) {
      map_user_id.push_back(user_ids[i]);
      map_group_id.push_back(group_ids[group]);
    }
  }
  co_await db.exec(ADD_GROUP_MAPPING_REQUEST);

  co_await db.commit();
  utils::ok<utils::empty_payload>(r, {});
}

ROUTE_REGISTER("/users/html-list", list_users_as_plain_text)
ROUTE_REGISTER("/users/replace", replace_users)
}  // namespace
