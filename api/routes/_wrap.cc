#include "stdafx.h"

#include "async/coro.h"
#include "async/pq.h"
#include "routes.h"
#include "utils/api.h"
using async::coro;

coro<void> perform_request(fcgx::request_t* r) noexcept {
  try {
    auto func = routes::route_storage::instance().get_route(*r);
    if (func == 0) {
      utils::err(r, api::INTERNAL_ERROR);
    } else {
      co_await func(r);
    }
#if KEGE_LOG_DEBUG_ENABLED
  } catch (utils::access_denied_error const&) {
  } catch (utils::expected_error const&) {
    async::detail::log_top_level_exception(std::current_exception());
#else
  } catch (utils::expected_error const&) {
#endif
  } catch (...) {
    if (!r->is_meta_fixed) {
      utils::err_nothrow(r, api::INTERNAL_ERROR);
    }
    async::detail::log_top_level_exception(std::current_exception());
  }
}
