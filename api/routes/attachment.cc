#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "routes/session.h"
#include "utils/api.h"
#include "utils/common.h"
using async::coro;

static coro<void> handle_attachment(fcgx::request_t *r) {
	const int BUFFER_SIZE = 8192;
	char buffer[BUFFER_SIZE];

	std::string hash = r->params["hash"], filename, mime_type;
	{
		auto db = co_await async::pq::connection_pool::local->get_connection();
		co_await routes::require_auth(db, r);
		tie(filename, mime_type) =
			(co_await db.exec("SELECT filename, mime_type FROM task_attachments WHERE hash = $1 "
							  "AND NOT coalesce(deleted, false)",
							  hash))
				.expect1<std::string, std::string>();
	}

	if (mime_type.starts_with("image/") || mime_type.starts_with("audio/") ||
		mime_type.starts_with("video/") || mime_type == "application/pdf") {
		r->headers.push_back("Content-Disposition: inline");
	} else {
		r->headers.push_back("Content-Disposition: attachment; filename*=UTF-8''" +
							 utils::url_encode(filename));
	}
	r->mime_type = mime_type;
	r->fix_meta();

	std::ifstream in(std::filesystem::path(conf.files_dir) / hash.substr(0, 2) / hash.substr(2),
					 std::ios::binary);
	while (in) {
		std::size_t sz = in.read(buffer, BUFFER_SIZE).gcount();
		r->out << std::string_view(buffer, buffer + sz);
	}
}

ROUTE_REGISTER("/attachment", handle_attachment)
