#pragma once

#include "stdafx.h"

#include "fcgx.h"

namespace utils {
std::string hmac_sign(std::string_view const& value, std::string_view const& key);
std::string sha3_256(std::string_view const& data);

std::string sign_url(std::string_view const& url,
                     std::vector<std::pair<std::string_view, std::string_view>> const& params,
                     bool full = true);
bool is_signed(fcgx::request_t* r, bool full = true) noexcept;
void signature_required(fcgx::request_t* r, bool full = true);

std::string urandom(int len);
std::string urandom_priv(int len);
}  // namespace utils