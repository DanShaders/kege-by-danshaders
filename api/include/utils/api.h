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

	void SerializeToString(const std::string *) const {}
};

template <typename T>
T expect(fcgx::request_t *r);

template <typename T>
void ok(fcgx::request_t *r, const typename T::initializable_type &response);

template <typename T>
void ok(fcgx::request_t *r, const T &response);

void send_raw(fcgx::request_t *r, api::ErrorCode code, const std::string &data);
[[noreturn]] void err(fcgx::request_t *r, api::ErrorCode code);
void err_nothrow(fcgx::request_t *r, api::ErrorCode code);

#if KEGE_LOG_DEBUG_ENABLED
class expected_error : public std::runtime_error {
public:
	expected_error() : runtime_error("") {}
	expected_error(const std::string &msg) : runtime_error(msg) {}
};

class access_denied_error : public expected_error {
	using expected_error::expected_error;
};

#else

class expected_error : public std::exception {
public:
	expected_error() : exception() {}
	expected_error(const std::string &) : exception() {}
};
#endif

#include "detail/api.impl.h"
}  // namespace utils