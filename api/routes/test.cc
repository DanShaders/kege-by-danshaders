#include "stdafx.h"

#include "KEGE.h"
#include "async/coro.h"
#include "async/curl.h"
#include "async/pq.h"
#include "routes.h"
#include "utils/api.h"
using async::coro;

static coro<void> handle_test(fcgx::request_t *r) {
	auto db = co_await async::pq::connection_pool::local->get_connection();

	for (auto [id, username] :
		 (co_await db.exec("SELECT id, username FROM users WHERE id <= $1", r->params["id"]))
			 .iter<int, std::string>()) {
		std::cout << id << " " << username << std::endl;
	}
	co_return;
}

ROUTE_REGISTER("/test", handle_test)
