#include "stdafx.h"

#include "KEGE.h"
#include "async/coro.h"
#include "async/curl.h"
#include "routes.h"
#include "utils/api.h"
using async::coro;

static coro<void> handle_status(fcgx::request_t *r) {
	utils::ok<api::StatusResponse>(
		r, {.version = KEGE_VERSION,
			.build_type = BUILD_TYPE,
			.server_time = std::chrono::system_clock::now().time_since_epoch() /
						   std::chrono::milliseconds(1)});
	co_return;
}

static coro<void> handle_fallback(fcgx::request_t *r) {
	utils::err(r, api::INVALID_SERVICE);
}

ROUTE_REGISTER("/status", handle_status)
ROUTE_FALLBACK_REGISTER(handle_fallback)
