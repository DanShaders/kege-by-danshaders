#pragma once

#include "stdafx.h"

#include "KEGE.h"
#include "async/coro.h"
#include "fcgx.h"
using async::coro;

namespace routes {
class route_storage;
class route_registrar;

class route_storage {
private:
	using func_t = coro<void> (*)(fcgx::request_t *);

	void *storage;

	route_storage();

public:
	~route_storage();

	static route_storage *instance();

	void add_route(const std::string_view &view, func_t func);
	void apply_root(const std::string_view &root);
	void set_fallback_route(func_t func);
	auto route_by_path(const std::string_view &view) -> func_t;
};

class route_registrar {
public:
	route_registrar(const std::string_view &view, coro<void> (*func)(fcgx::request_t *)) {
		route_storage::instance()->add_route(view, func);
	}
};

class route_fallback_registrar {
public:
	route_fallback_registrar(coro<void> (*func)(fcgx::request_t *)) {
		route_storage::instance()->set_fallback_route(func);
	}
};

#define ROUTE_REGISTER_CONCAT_IMPL(a, b) a##b
#define ROUTE_REGISTER_CONCAT(a, b) ROUTE_REGISTER_CONCAT_IMPL(a, b)

#define ROUTE_REGISTER(path, handle)                                           \
	static auto ROUTE_REGISTER_CONCAT(route_registrar_autogen_, __COUNTER__) = \
		routes::route_registrar(path, handle);
#define ROUTE_FALLBACK_REGISTER(handle)                                        \
	static auto ROUTE_REGISTER_CONCAT(route_registrar_autogen_, __COUNTER__) = \
		routes::route_fallback_registrar(handle);
}  // namespace routes