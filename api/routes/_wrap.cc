#include "stdafx.h"

#include "async/coro.h"
#include "routes.h"
#include "utils/api.h"
using async::coro;

coro<void> perform_request(fcgx::request_t *r) noexcept {
	try {
		auto func = routes::route_storage::instance()->route_by_path(r->request_uri);
		if (func == 0)
			utils::err(r, api::INTERNAL_ERROR);
		else
			co_await func(r);
	} catch (const utils::expected_error &) {
	} catch (const std::exception &e) {
		async::detail::log_top_level_exception(e.what());
	} catch (...) { async::detail::log_top_level_exception(); }
}