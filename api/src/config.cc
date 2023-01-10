#include "config.h"
using namespace config;

void config::from_json(json const& j, hl_socket_address& obj) {
  bool& flag = obj.use_unix_sockets;
  j.at("use_unix_sockets").get_to(flag);
  j.at("path").get_to(obj.path);
  if (!flag) {
    j.at("port").get_to(obj.port);
  } else {
    if (j.contains("perms")) {
      j.at("perms").get_to(obj.perms);
    } else {
      obj.perms = -1;
    }
  }
}

void config::from_json(json const& j, config_t& obj) {
  j.at("workers").get_to(obj.request_workers);
  j.at("files_dir").get_to(obj.files_dir);
  j.at("api_root").get_to(obj.api_root);
  j.at("fastcgi").get_to(obj.fastcgi);
  j.at("db").get_to(obj.db);
}

config_t config::find_config(std::filesystem::path const& path) {
  auto conf_path = path / "config.json";
  logi("Loading configuration from {}...", conf_path.string());
  std::ifstream conf_stream;
  conf_stream.exceptions(conf_stream.exceptions() | std::ios::failbit);
  conf_stream.open(conf_path);
  std::string conf_data{std::istreambuf_iterator<char>(conf_stream),
                        std::istreambuf_iterator<char>()};
  auto lconf = json::parse(conf_data, 0, true, true).get<config_t>();
  lconf.root = path;
  return lconf;
}
