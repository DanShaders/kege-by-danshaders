#include "routes.h"
using namespace routes;

/* ==== routes::route_storage ==== */
/** @private */
struct route_storage::storage_t {
public:
  struct node {
    std::map<std::string, std::unique_ptr<node>, std::less<>> go;
    route_t current = nullptr;
    std::unique_ptr<std::pair<std::string, node>> var;
  };

private:
  constexpr static std::string_view DELIM{"/"};

  void append_var(node*& n, std::set<std::string, std::less<>>& vars, std::string_view name) {
    if (vars.count(name)) {
      throw std::invalid_argument(std::string(name) + " is not unique amongst variables");
    }
    vars.insert(std::string(name));

    if (n->var) {
      if (n->var->first != name) {
        throw std::invalid_argument("node already has catch-all with different name");
      }
    } else {
      n->var = std::make_unique<std::pair<std::string, node>>(name, node{});
    }
    n = &n->var->second;
  }

  void append_part(node*& n, std::string_view part) {
    auto it = n->go.find(part);
    if (it == n->go.end()) {
      n = (n->go[std::string(part)] = std::make_unique<node>()).get();
    } else {
      n = it->second.get();
    }
  }

public:
  bool is_built = false;
  std::vector<std::pair<std::string, route_t>> raw;

  std::unique_ptr<node> root;

  route_t fallback = 0;

  void insert_path(std::string const& path, route_t route) {
    auto n = root.get();
    std::set<std::string, std::less<>> vars;

    std::string_view path_sv{path};
    for (auto part_v : std::views::split(path_sv.substr(1), DELIM)) {
      std::string_view part{part_v.begin(), part_v.end()};

      if (part.empty() || part == "." || part == ".." || part == "$") {
        throw std::invalid_argument("encountered invalid path segment");
      }
      if (part[0] == '$') {
        append_var(n, vars, part.substr(1));
      } else {
        append_part(n, part);
      }
    }

    if (n->current) {
      throw std::invalid_argument("path already exists");
    }
    n->current = route;
  }

  route_t get_route(std::string_view path, std::map<std::string, std::string, std::less<>>& vars) {
    auto n = root.get();

    for (auto part_v : std::views::split(path.substr(1), DELIM)) {
      std::string_view part{part_v.begin(), part_v.end()};

      auto it = n->go.find(part);
      if (it == n->go.end()) {
        if (n->var) {
          vars[n->var->first] = part;
          n = &n->var->second;
        } else {
          return fallback;
        }
      } else {
        n = it->second.get();
      }
    }
    return n->current ? n->current : fallback;
  }
};

route_storage::route_storage() : s(std::make_unique<storage_t>()) {}

route_storage& route_storage::instance() {
  static std::unique_ptr<route_storage> inst;
  if (!inst) {
    inst = std::unique_ptr<route_storage>(new route_storage());
  }
  return *inst.get();
}

void route_storage::add_route(std::string_view view, route_t route) {
  assert(!s->is_built);
  assert(view.size() > 1 && view[0] == '/');

  s->raw.emplace_back(std::string(view), route);
}

void route_storage::build(std::string_view api_root) {
  assert(!s->is_built);
  s->is_built = true;

  if (api_root.size() && api_root[0] != '/') {
    throw std::invalid_argument("API root must not be relative");
  }

  s->root = std::make_unique<storage_t::node>();

  for (auto& [path, route] : s->raw) {
    path.insert(0, api_root);
    try {
      s->insert_path(path, route);
    } catch (std::invalid_argument const& e) {
      std::throw_with_nested(std::runtime_error("inserting " + path + " failed"));
    }
  }
}

void route_storage::set_fallback_route(route_t route) {
  assert(!s->is_built);
  s->fallback = route;
}

route_t route_storage::get_route(fcgx::request_t& r) {
  assert(s->is_built);
  return s->get_route(r.request_uri, r.params);
}
