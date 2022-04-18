#pragma once

#include "stdafx.h"

namespace utils {
std::string url_decode(const std::string_view &);
std::string url_encode(const std::string_view &);

std::map<std::string, std::string> parse_query_string(const std::string_view &);
std::map<std::string, std::string> parse_cookies(const char *s);

std::string b16_decode(const std::string_view &);
std::string b16_encode(const std::string_view &);
std::string b64_decode(const std::string_view &);
std::string b64_encode(const std::string_view &);

std::string replace_all(const std::string_view &, const std::string_view &,
						const std::string_view &);

inline void ensure(bool cond, const std::string &msg, int err = errno) {
	if (cond) [[unlikely]] {
		throw std::system_error(err, std::system_category(), msg);
	}
}

template <typename T>
class scope_guard {
private:
	T func;

public:
	scope_guard(T func_) : func(func_) {}

	~scope_guard() {
		func();
	}
};
}  // namespace utils