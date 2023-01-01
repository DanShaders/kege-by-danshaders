#pragma once

#include "stdafx.h"

namespace config {
struct hl_socket_address {
  bool use_unix_sockets;
  std::string path;
  unsigned short port;
  int perms;
};

struct db_info_t {
  std::string path;
  std::size_t connections;
  long long cooldown;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(db_info_t, path, connections, cooldown)
};

struct config_t {
  std::size_t request_workers;
  std::string api_root;
  std::string files_dir;
  std::filesystem::path root;
  hl_socket_address fastcgi;
  db_info_t db;
};

void from_json(json const& j, hl_socket_address& o);
void from_json(json const& j, config_t& o);
config_t find_config(std::filesystem::path const& path);
}  // namespace config

inline config::config_t conf;