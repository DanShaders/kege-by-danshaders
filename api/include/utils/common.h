#pragma once

#include "stdafx.h"

namespace utils {
std::string url_decode(const std::string_view &);
std::string url_encode(const std::string_view &);

std::map<std::string, std::string> parse_query_string(const std::string_view &);

std::string b64_decode(const std::string_view &);
std::string b64_encode(const std::string_view &);

std::string replace_all(const std::string_view &, const std::string_view &,
						const std::string_view &);
}  // namespace utils