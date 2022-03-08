#pragma once

#include "stdafx.h"

namespace config {
struct hl_socket_address {
	bool use_unix_sockets;
	std::string path;
	unsigned short port;
	int perms;
};

struct config_t {
	std::size_t request_workers;
	std::string api_root;
	std::filesystem::path root;
	hl_socket_address fastcgi;
	std::string db_path;
	std::size_t db_connections;
};

void from_json(const json &j, hl_socket_address &o);
void from_json(const json &j, config_t &o);
config_t find_config(const std::filesystem::path &path);
}  // namespace config

inline config::config_t conf;