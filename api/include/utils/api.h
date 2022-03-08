#pragma once

#include "stdafx.h"

#include "api.pb.h"
#include "fcgx.h"

namespace utils {
template <typename T>
T expect(fcgx::request_t *r);

template <typename T>
void ok(fcgx::request_t *r, const typename T::initializable_type &response);

void send_raw(fcgx::request_t *r, api::ErrorCode code, const std::string &data);
[[noreturn]] void err(fcgx::request_t *r, api::ErrorCode code);
void err_nothrow(fcgx::request_t *r, api::ErrorCode code);

class expected_error : public std::exception {};

#include "detail/api.impl.h"
}  // namespace utils