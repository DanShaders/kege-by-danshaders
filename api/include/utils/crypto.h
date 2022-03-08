#pragma once

#include "stdafx.h"

#include "fcgx.h"

namespace utils {
std::string hmac_sign(const std::string_view &value, const std::string_view &key);

std::string sign_url(const std::string_view &url,
					 const std::vector<std::pair<std::string_view, std::string_view>> &params,
					 bool full = true);
bool is_signed(fcgx::request_t *r, bool full = true) noexcept;
void signature_required(fcgx::request_t *r, bool full = true);

std::string urandom(int len);
std::string urandom_priv(int len);
}  // namespace utils