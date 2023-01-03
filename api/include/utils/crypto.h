#pragma once

#include "stdafx.h"

#include "fcgx.h"

namespace utils {
inline constexpr size_t signature_length = 32;
using signature_t = std::array<char, signature_length>;

signature_t hmac_sign(std::string_view value, std::string_view key);
std::string sha3_256(std::string_view data);

std::string urandom(int length);
std::string urandom_priv(int length);
}  // namespace utils