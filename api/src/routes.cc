#include "routes.h"
using namespace routes;

/* ==== routes::route_storage ==== */
using func_t = coro<void> (*)(fcgx::request_t *);

struct internal_storage {
	std::map<std::string, func_t, std::less<>> routes;
	func_t fallback = 0;
};

route_storage::route_storage() : storage(new internal_storage{}) {}

route_storage::~route_storage() {
	delete (internal_storage *) storage;
}

route_storage *route_storage::instance() {
	static std::unique_ptr<route_storage> inst;
	if (!inst)
		inst = std::unique_ptr<route_storage>(new route_storage());
	return inst.get();
}

void route_storage::add_route(const std::string_view &view, func_t func) {
	auto s = (internal_storage *) storage;

	s->routes[std::string(view)] = func;
}

void route_storage::apply_root(const std::string_view &root) {
	auto s = (internal_storage *) storage;

	decltype(s->routes) mod;
	for (auto [route, func] : s->routes) {
		mod[std::string(root) + route] = func;
	}
	s->routes = mod;
}

void route_storage::set_fallback_route(func_t func) {
	auto s = (internal_storage *) storage;

	s->fallback = func;
}

auto route_storage::route_by_path(const std::string_view &view) -> func_t {
	auto s = (internal_storage *) storage;

	auto it = s->routes.find(view);
	if (it != s->routes.end())
		return it->second;
	return s->fallback;
}