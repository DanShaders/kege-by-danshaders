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
	std::filesystem::path root;
	hl_socket_address fastcgi;
	db_info_t db;
};

void from_json(const json &j, hl_socket_address &o);
void from_json(const json &j, config_t &o);
config_t find_config(const std::filesystem::path &path);
}  // namespace config

inline config::config_t conf;