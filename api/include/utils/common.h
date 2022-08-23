/**
 * General utilities (mostly related to string)
 * @file
 */
#pragma once

#include "stdafx.h"

/** Utilities */
namespace utils {
/** Decodes string in URL-encoded format. */
std::string url_decode(const std::string_view &);

/** Encodes string to URL-encoded format. */
std::string url_encode(const std::string_view &);

using parameters_map = std::map<std::string, std::string, std::less<>>;

/**
 * Parses URL search params.
 *
 * @param[in]  <unnamed>  URL search params without leading '?'
 *
 * @return     map from parameter name to its values. In case of duplicate parameter names the
 *             last one is used. Empty names/values are accepted.
 */
parameters_map parse_query_string(const std::string_view &);

/**
 * Parses cookie header from client.
 *
 * @param[in]  s     Header value
 *
 * @return     map from cookie name to its values. In case of duplicate parameter names the last one
 *             is used. Empty names/values are accepted.
 */
parameters_map parse_cookies(const char *s);

/** Encodes string into lowercase hex */
std::string b16_decode(const std::string_view &);

/** Decodes string from hex, case-insensetive. */
std::string b16_encode(const std::string_view &);

/** Decodes string from base64 with alphabet `A-Za-z0-9+/` */
std::string b64_decode(const std::string_view &);

/** Encodes string into base64 with alphabet `A-Za-z0-9+/` */
std::string b64_encode(const std::string_view &);

/**
 * Replaces all occurrences of @p from by @p to in @p s.
 *
 * @param[in]  s     Original string
 * @param[in]  from  String to be replaced by @p to
 * @param[in]  to    String that replaces @p from
 *
 * @return     String with replaced substrings.
 */
std::string replace_all(const std::string_view &s, const std::string_view &from,
						const std::string_view &to);

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

template <typename T>
auto millis_since_epoch(T timepoint) {
	return timepoint.time_since_epoch() / std::chrono::milliseconds(1);
}
}  // namespace utils