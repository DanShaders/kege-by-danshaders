#pragma once

#include "stdafx.h"

#include "KEGE.h"
#include "api.pb.h"
#include "fcgx.h"

namespace utils {
struct empty_payload {
  struct initializable_type {};

  empty_payload() {}
  empty_payload(initializable_type) {}

  void SerializeToString(std::string const*) const {}
};

void send_raw(fcgx::request_t* r, api::ErrorCode code, std::string_view data);

template <typename T>
void ok(fcgx::request_t* r, const typename T::initializable_type& response) {
  std::string data;
  T{response}.SerializeToString(&data);
  send_raw(r, api::OK, data);
}

template <typename T>
void ok(fcgx::request_t* r, T const& response) {
  std::string data;
  response.SerializeToString(&data);
  send_raw(r, api::OK, data);
}

[[noreturn]] void err(fcgx::request_t* r, api::ErrorCode code);

void err_nothrow(fcgx::request_t* r, api::ErrorCode code);

template <typename T>
T expect(fcgx::request_t* r) {
  T result;
  if (!result.ParseFromString(r->raw_body)) {
    err(r, api::INVALID_QUERY);
  }
  return result;
}

template <typename T>
T expect(fcgx::request_t* r, std::string_view s) {
  auto param_it = r->params.find(s);
  if (param_it == r->params.end()) {
    err(r, api::INVALID_QUERY);
  }
  auto const& param = param_it->second;
  T result;
  if (std::from_chars(param.data(), param.data() + param.size(), result).ec != std::errc()) {
    err(r, api::INVALID_QUERY);
  }
  return result;
}

#if KEGE_LOG_DEBUG_ENABLED
class expected_error : public std::runtime_error {
public:
  expected_error() : runtime_error("") {}
  expected_error(std::string const& msg) : runtime_error(msg) {}
};

class access_denied_error : public expected_error {
  using expected_error::expected_error;
};

#else

class expected_error : public std::exception {
public:
  expected_error() : exception() {}
  expected_error(std::string const&) : exception() {}
};
#endif
}  // namespace utils