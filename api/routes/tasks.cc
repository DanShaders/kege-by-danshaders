#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "tasks.pb.h"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"
#include "utils/html.h"
using async::coro;

namespace {
coro<void> handle_get(fcgx::request_t *r) {
	auto db = co_await async::pq::connection_pool::local->get_connection();
	co_await routes::require_auth(db, r, routes::PERM_VIEW_TASKS);

	auto q = co_await db.exec(
		"SELECT * FROM tasks WHERE id = $1::bigint AND NOT coalesce(deleted, false)",
		r->params["id"]);
	if (!q.rows()) {
		utils::ok<api::Task>(r, {});
		co_return;
	}

	auto [id, task_type, parent, task, tag, answer_rows, answer_cols, answer, deleted] =
		q.expect1<int64_t, int64_t, int64_t, std::string_view, std::string_view, int, int,
				  std::string_view, bool>();

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
		"$1::bigint AND NOT coalesce(deleted, false)",
		r->params["id"]);
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
	  "tag = (CASE WHEN $15 THEN excluded ELSE tasks END).tag";

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
// clang-format on

const lxb_char_t *operator"" _u(const char *str, size_t) {
	return (const lxb_char_t *) str;
}

const std::vector<utils::transform_rule> RULES_GATHER_IDS = {
	{"img[data-id]",
	 [](lxb_dom_node_t *node, void *ctx) {
		 auto links = (std::vector<int64_t> *) ctx;

		 size_t attr_length;
		 auto elem = lxb_dom_interface_element(node);
		 auto value =
			 (const char *) lxb_dom_element_get_attribute(elem, "data-id"_u, 7, &attr_length);

		 int64_t id = -1;
		 std::from_chars(value, value + attr_length, id);
		 links->push_back(id);
		 return node;
	 }},
};

const std::vector<utils::transform_rule> RULES_SANITIZE = {
	{"img",
	 [](lxb_dom_node_t *node, void *ctx) -> lxb_dom_node_t * {
		 auto id_map = (std::map<int64_t, std::string> *) ctx;

		 size_t attr_length;
		 auto elem = lxb_dom_interface_element(node);
		 auto value =
			 (const char *) lxb_dom_element_get_attribute(elem, "data-id"_u, 7, &attr_length);

		 int64_t id = -1;
		 std::from_chars(value, value + attr_length, id);

		 if (auto it = id_map->find(id); it != id_map->end()) {
			 auto url = "api/attachment/" + it->second;
			 lxb_dom_element_set_attribute(elem, "src"_u, 3, (const lxb_char_t *) url.data(),
										   url.size());
			 return node;
		 } else {
			 return nullptr;
		 }
	 }},
	// TODO: actually sanitize
};

utils::transform_rules TRANSFORM_GATHER_IDS = nullptr;
utils::transform_rules TRANSFORM_SANITIZE = nullptr;

void compile_rules() {
	TRANSFORM_GATHER_IDS = utils::compile_transform_rules(RULES_GATHER_IDS);
	TRANSFORM_SANITIZE = utils::compile_transform_rules(RULES_SANITIZE);
}

coro<void> handle_update(fcgx::request_t *r) {
	auto db = co_await async::pq::connection_pool::local->get_connection();
	co_await routes::require_auth(db, r, routes::PERM_WRITE_TASKS);

	auto task = utils::expect<api::Task>(r);
	if (!task.id()) {
		utils::err(r, api::INVALID_QUERY);
	}
	std::vector<std::pair<std::string, const std::string &>> files;

	co_await db.transaction();

	for (const auto &attachment : task.attachments()) {
		std::string hash = SHA3_256_EMPTY;
		if (attachment.contents().size()) {
			hash = utils::b16_encode(utils::sha3_256(attachment.contents()));
		}
		auto [is_inserted] =
			(co_await db.exec(ATTACHMENT_UPDATE_SQL, attachment.id(), task.id(),
							  attachment.filename(), attachment.has_filename(),
							  attachment.mime_type(), attachment.has_mime_type(),
							  attachment.shown_to_user(), attachment.has_shown_to_user(),
							  attachment.deleted(), attachment.has_deleted(), hash))
				.expect1<int>();
		if (is_inserted) {
			files.push_back({hash, attachment.contents()});
		}
	}

	std::string task_text;
	if (task.has_text()) {
		if (!TRANSFORM_GATHER_IDS) {
			compile_rules();
		}

		utils::html_fragment text(task.text());
		std::vector<int64_t> ids;
		text.transform(TRANSFORM_GATHER_IDS, &ids);

		std::map<int64_t, std::string> id_map;
		if (ids.size()) {
			std::string query =
				"SELECT id, hash FROM task_attachments WHERE task_id = $1 AND id IN (VALUES ";
			for (auto id : ids) {
				query += "(" + std::to_string(id) + "),";
			}
			query.pop_back();
			query.push_back(')');
			auto q = co_await db.exec(query.data(), task.id());
			for (auto [id, hash] : q.iter<int64_t, std::string_view>()) {
				id_map[id] = hash;
			}
		}
		text.transform(TRANSFORM_SANITIZE, &id_map);
		task.set_text(text.get_html());
	}

	co_await db.exec(TASK_UPDATE_SQL, task.id(), task.task_type(), task.has_task_type(),
					 task.parent(), task.has_parent(), task.text(), task.has_text(),
					 task.answer_rows(), task.has_answer_rows(), task.answer_cols(),
					 task.has_answer_cols(), task.answer(), task.has_answer(), task.tag(),
					 task.has_tag());

	co_await db.commit();

	for (const auto &[hash, content] : files) {
		auto path = std::filesystem::path(conf.files_dir) / hash.substr(0, 2);
		std::filesystem::create_directories(path);
		std::ofstream out(path / hash.substr(2), std::ios::binary);
		out << content;
	}
	utils::ok(r, utils::empty_payload{});
}
}  // namespace

ROUTE_REGISTER("/tasks/$id", handle_get)
ROUTE_REGISTER("/tasks/update", handle_update)
