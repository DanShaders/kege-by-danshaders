#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "tasks.pb.h"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"
#include "utils/filter.h"
#include "utils/html.h"
using async::coro;

namespace {
coro<void> handle_get(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await routes::require_auth(db, r, routes::PERM_ADMIN);

  int64_t task_id = utils::expect<int64_t>(r, "id");

  auto q = co_await db.exec("SELECT * FROM tasks WHERE id = $1", task_id);
  if (!q.rows()) {
    utils::ok<api::Task>(r, {});
    co_return;
  }

  auto [id, task_type, parent, task, tag, answer_rows, answer_cols, answer, deleted] =
      q.expect1<int64_t, int64_t, int64_t, std::string_view, std::string_view, int, int,
                std::string_view, bool>();

  if (deleted) {
    utils::err(r, api::INVALID_QUERY);
  }

  api::Task msg{{
      .id = id,
      .task_type = task_type,
      .parent = parent,
      .text = std::string(task),
      .answer_rows = answer_rows,
      .answer_cols = answer_cols,
      .answer = std::string(answer),
      .tag = std::string(tag),
  }};

  auto q2 = co_await db.exec(
      "SELECT id, filename, mime_type, hash, shown_to_user FROM task_attachments WHERE task_id = "
      "$1 AND NOT coalesce(deleted, false)",
      task_id);
  for (auto [attachment_id, filename, mime_type, hash, shown_to_user] :
       q2.iter<int64_t, std::string_view, std::string_view, std::string_view, bool>()) {
    *msg.add_attachments() = {{
        .id = attachment_id,
        .filename = std::string(filename),
        .mime_type = std::string(mime_type),
        .hash = std::string(hash),
        .shown_to_user = shown_to_user,
    }};
  }

  utils::ok(r, msg);
}

// clang-format off
const char SHA3_256_EMPTY[] =
	"a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a";

const char TASK_UPDATE_SQL[] =
	"INSERT INTO tasks ("
	  "id, task_type, parent, task, answer_rows, "
	  "answer_cols, answer, tag"
	") "
	"VALUES "
	  "("
	    "$1, "
	    "(CASE WHEN $3 THEN $2::bigint ELSE NULL END), "
	    "(CASE WHEN $5 THEN $4::bigint ELSE NULL END), "
	    "(CASE WHEN $7 THEN $6 ELSE NULL END), "
	    "(CASE WHEN $9 THEN $8::integer ELSE NULL END), "
	    "(CASE WHEN $11 THEN $10::integer ELSE NULL END), "
	    "(CASE WHEN $13 THEN $12::bytea ELSE NULL END), "
	    "(CASE WHEN $15 THEN $14 ELSE NULL END)"
	  ")"
	"ON CONFLICT (id) DO UPDATE SET "
	  "task_type = (CASE WHEN $3 THEN excluded ELSE tasks END).task_type, "
	  "parent = (CASE WHEN $5 THEN excluded ELSE tasks END).parent, "
	  "task = (CASE WHEN $7 THEN excluded ELSE tasks END).task, "
	  "answer_rows = (CASE WHEN $9 THEN excluded ELSE tasks END).answer_rows, "
	  "answer_cols = (CASE WHEN $11 THEN excluded ELSE tasks END).answer_cols, "
	  "answer = (CASE WHEN $13 THEN excluded ELSE tasks END).answer, "
	  "tag = (CASE WHEN $15 THEN excluded ELSE tasks END).tag "
	"RETURNING (xmax = 0)";

const char ATTACHMENT_UPDATE_SQL[] =
	"INSERT INTO task_attachments ("
	  "id, task_id, filename, hash, mime_type, "
	  "shown_to_user, deleted"
	") "
	"VALUES "
	  "("
	    "$1, "
	    "$2, "
	    "(CASE WHEN $4 THEN $3 ELSE NULL END), "
	    "$11, "
	    "(CASE WHEN $6 THEN $5 ELSE NULL END), "
	    "(CASE WHEN $8 THEN $7::bool ELSE NULL END), "
	    "(CASE WHEN $10 THEN $9::bool ELSE NULL END)"
	  ")"
	"ON CONFLICT (id) DO UPDATE SET "
	  "filename = (CASE WHEN $4 THEN excluded ELSE task_attachments END).filename, "
	  "mime_type = (CASE WHEN $6 THEN excluded ELSE task_attachments END).mime_type, "
	  "shown_to_user = (CASE WHEN $8 THEN excluded ELSE task_attachments END).shown_to_user, "
	  "deleted = (CASE WHEN $10 THEN excluded ELSE task_attachments END).deleted "
	"WHERE "
	  "task_attachments.task_id = excluded.task_id "
	"RETURNING (xmax = 0)";

const char TASK_LIST_SQL[] =
	"SELECT "
	  "tasks.id, task_types.short_name, tasks.tag "
	"FROM "
	  "tasks "
	  "INNER JOIN task_types ON tasks.task_type = task_types.id "
	"WHERE "
	  "NOT coalesce(tasks.deleted, false)"
	"ORDER BY "
	  "id";

const char TASK_BULK_DELETE_SQL[] =
	"UPDATE "
	"  tasks "
	"SET "
	"  deleted = true "
	"FROM "
	"  unnest($1::bigint[]) as ids "
	"WHERE "
	"  tasks.id = ids";
// clang-format on

lxb_char_t const* operator"" _u(char const* str, size_t) {
  return (lxb_char_t const*) str;
}

const std::vector<std::string> ALLOWED_TAGS = {
    "a", "b", "i", "s", "u", "div", "img", "font", "sub", "sup", "br", "formula", "pre",
};

auto const CATCH_ALL_SELECTOR = ([] {
  std::string slctr;
  for (auto tag : ALLOWED_TAGS) {
    slctr += ":not(" + tag + ")";
  }
  return slctr;
})();

auto use_tag(std::map<std::string_view, std::set<std::string_view>> allowed_attrs = {}) {
  return [=](lxb_dom_node_t* node, void*) {
    auto elem = lxb_dom_interface_element(node);

    for (auto attr = elem->first_attr; attr;) {
      size_t key_size, value_size;
      auto key = (const char*) lxb_dom_attr_qualified_name(attr, &key_size);
      auto value = (const char*) lxb_dom_attr_value(attr, &value_size);
      auto next = attr->next;

      auto it = allowed_attrs.find({key, key_size});
      bool is_allowed = true;
      if (it != allowed_attrs.end()) {
        if (it->second.size() && !it->second.count({value, value_size})) {
          is_allowed = false;
        }
      } else {
        is_allowed = false;
      }
      if (!is_allowed) {
        lxb_dom_element_attr_remove(elem, attr);
      }
      attr = next;
    }

    return node;
  };
}

const std::vector<utils::transform_rule> RULES_SANITIZE = {
    {"img",
     [](lxb_dom_node_t* node, void* ctx) -> lxb_dom_node_t* {
       auto id_map = (std::map<int64_t, std::string>*) ctx;

       size_t attr_length;
       auto elem = lxb_dom_interface_element(node);
       auto value = (const char*) lxb_dom_element_get_attribute(elem, "data-id"_u, 7, &attr_length);

       int64_t id = -1;
       std::from_chars(value, value + attr_length, id);

       if (auto it = id_map->find(id); it != id_map->end()) {
         auto url = "/api/attachment/" + it->second;
         lxb_dom_element_set_attribute(elem, "src"_u, 3, (const lxb_char_t*) url.data(),
                                       url.size());
         return node;
       } else {
         return nullptr;
       }
     }},

    {"br, b, i, u, s, sub, sup, pre, formula", use_tag()},
    {"div", use_tag({{"align", {"left", "center", "right", "justify"}}})},
    {"img", use_tag({{"src", {}}})},
    {"a", use_tag({{"href", {}}})},
    {"font", use_tag({{"size", {"1", "2", "3", "4", "5", "6"}}, {"color", {}}})},
    {CATCH_ALL_SELECTOR, [](auto...) { return nullptr; }},
};

std::atomic<utils::transform_rules> TRANSFORM_SANITIZE = nullptr;

coro<void> handle_update(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  auto session = co_await routes::require_auth(db, r, routes::PERM_ADMIN);

  auto task = utils::expect<api::Task>(r);
  if (!task.id()) {
    utils::err(r, api::INVALID_QUERY);
  }
  std::vector<std::pair<std::string, std::string const&>> files;
  std::map<int64_t, std::string> id_map;

  co_await db.transaction();

  for (auto const& attachment : task.attachments()) {
    std::string hash = SHA3_256_EMPTY;
    if (attachment.contents().size()) {
      hash = utils::b16_encode(utils::sha3_256(attachment.contents()));
    }
    id_map[attachment.id()] = hash;
  }

  std::string task_text;
  if (task.has_text()) {
    if (!TRANSFORM_SANITIZE.load()) {
      // Might be called concurrently, seems harmless
      TRANSFORM_SANITIZE = utils::compile_transform_rules(RULES_SANITIZE);
    }

    utils::html_fragment text(task.text());
    auto q =
        co_await db.exec("SELECT id, hash FROM task_attachments WHERE task_id = $1", task.id());
    for (auto [id, hash] : q.iter<int64_t, std::string_view>()) {
      id_map[id] = hash;
    }
    text.transform(TRANSFORM_SANITIZE, &id_map);
    task.set_text(text.get_html());
  }

  auto [is_task_inserted] =
      (co_await db.exec(TASK_UPDATE_SQL, task.id(), task.task_type(), task.has_task_type(),
                        task.parent(), task.has_parent(), task.text(), task.has_text(),
                        task.answer_rows(), task.has_answer_rows(), task.answer_cols(),
                        task.has_answer_cols(), task.answer(), task.has_answer(), task.tag(),
                        task.has_tag()))
          .expect1<bool>();

  if (is_task_inserted && !is_id_owned_by(session, task.id())) {
    // Either we are f*cked up or somebody is trying to fool us.
    utils::err(r, api::EXTREMELY_SORRY);
  }

  for (auto const& attachment : task.attachments()) {
    auto hash = id_map[attachment.id()];
    auto [is_inserted] =
        (co_await db.exec(ATTACHMENT_UPDATE_SQL, attachment.id(), task.id(), attachment.filename(),
                          attachment.has_filename(), attachment.mime_type(),
                          attachment.has_mime_type(), attachment.shown_to_user(),
                          attachment.has_shown_to_user(), attachment.deleted(),
                          attachment.has_deleted(), hash))
            .expect1<bool>();
    if (is_inserted) {
      if (!is_id_owned_by(session, attachment.id())) {
        utils::err(r, api::EXTREMELY_SORRY);
      }
      files.push_back({hash, attachment.contents()});
    }
  }

  co_await db.commit();

  for (auto const& [hash, content] : files) {
    auto path = std::filesystem::path(conf.files_dir) / hash.substr(0, 2);
    std::filesystem::create_directories(path);
    std::ofstream out(path / hash.substr(2), std::ios::binary);
    out << content;
  }
  utils::ok(r, utils::empty_payload{});
}

coro<void> handle_list(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await routes::require_auth(db, r, routes::PERM_ADMIN);

  auto req = utils::expect<api::TaskListRequest>(r);

  // clang-format off
	// !refs && (type == 19 || type == 20 || type == 21) && parent_included && type_count <= 4
	utils::filter filter(req.filter(), {
		{"id", utils::T_INTEGER},
		{"type", utils::T_INTEGER},
		// refs - number of reference in KIMs
		// parent_included - does this filter have parent
		// filtered already type_count - number of tasks with the
		// same type filtered already tag - comment
	});
  // clang-format on
  if (auto compile_error = filter.get_error()) {
    utils::send_raw(r, api::INVALID_QUERY, *compile_error);
    throw utils::expected_error("unable to compile filter");
  }

  api::TaskListResponse msg{{
      .page_count = 1,
      .has_more_pages = false,
  }};

  auto q = co_await db.exec(TASK_LIST_SQL);
  for (auto [id, task_type, tag] : q.iter<int64_t, int, std::string_view>()) {
    if (!filter.matches({id, task_type})) {
      continue;
    }
    *msg.add_tasks() = {{
        .id = id,
        .task_type = task_type,
        .tag = std::string(tag),
    }};
  }

  utils::ok(r, msg);
}

coro<void> handle_bulk_delete(fcgx::request_t* r) {
  auto db = co_await async::pq::connection_pool::local->get_connection();
  co_await routes::require_auth(db, r, routes::PERM_ADMIN);

  auto req = utils::expect<api::TaskBulkDeleteRequest>(r);
  co_await db.exec(TASK_BULK_DELETE_SQL, req.tasks());

  utils::ok<utils::empty_payload>(r, {});
}
}  // namespace

ROUTE_REGISTER("/tasks/$id", handle_get)
ROUTE_REGISTER("/tasks/update", handle_update)
ROUTE_REGISTER("/tasks/list", handle_list)
ROUTE_REGISTER("/tasks/bulk-delete", handle_bulk_delete)
