#include "stdafx.h"

#include "KEGE.h"
#include "async/coro.h"
#include "async/curl.h"
#include "routes.h"
#include "utils/api.h"
using async::coro;

static coro<void> handle_test(fcgx::request_t *r) {
	// std::shared_ptr<PGconn> conn{PQconnectdb(conf.db_path.c_str()), PQfinish};
	// if (PQstatus(conn.get()) != CONNECTION_OK) {
	// 	throw std::runtime_error("PQ connection failed");
	// }
	// PQfinish(conn);
	co_return;
}

ROUTE_REGISTER("/test", handle_test)
