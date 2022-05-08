#pragma once

#include "stdafx.h"

#include "KEGE.h"
#include "async/coro.h"
#include "fcgx.h"
using async::coro;

namespace routes {
using route_t = coro<void> (*)(fcgx::request_t *);
class route_storage;
class route_registrar;

class route_storage {
private:
	struct storage_t;
	std::unique_ptr<storage_t> s;

	route_storage();

public:
	static route_storage &instance();

	void add_route(std::string_view view, route_t route);
	void build(std::string_view api_root);
	route_t get_route(fcgx::request_t &r);
	void set_fallback_route(route_t route);
};

class route_registrar {
public:
	route_registrar(std::string_view view, route_t route) {
		route_storage::instance().add_route(view, route);
	}
};

class route_fallback_registrar {
public:
	route_fallback_registrar(route_t route) {
		route_storage::instance().set_fallback_route(route);
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