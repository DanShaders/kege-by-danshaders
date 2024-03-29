/**
 * General utilities (mostly related to string)
 * @file
 */
#pragma once

#include "stdafx.h"

/** Utilities */
namespace utils {
/** Decodes string in URL-encoded format. */
std::string url_decode(std::string_view const&);

/** Encodes string to URL-encoded format. */
std::string url_encode(std::string_view const&);

using parameters_map = std::map<std::string, std::string, std::less<>>;

/**
 * Parses URL search params.
 *
 * @param[in]  <unnamed>  URL search params without leading '?'
 *
 * @return     map from parameter name to its values. In case of duplicate parameter names the
 *             last one is used. Empty names/values are accepted.
 */
parameters_map parse_query_string(std::string_view const&);

/**
 * Parses cookie header from client.
 *
 * @param[in]  s     Header value
 *
 * @return     map from cookie name to its values. In case of duplicate parameter names the last one
 *             is used. Empty names/values are accepted.
 */
parameters_map parse_cookies(char const* s);

/** Encodes string into lowercase hex */
std::string b16_decode(std::string_view const&);

/** Decodes string from hex, case-insensetive. */
std::string b16_encode(std::string_view const&);

/** Decodes string from base64 with alphabet `A-Za-z0-9+/` */
std::string b64_decode(std::string_view const&);

/** Encodes string into base64 with alphabet `A-Za-z0-9+/` */
std::string b64_encode(std::string_view const&);

/**
 * Replaces all occurrences of @p from by @p to in @p s.
 *
 * @param[in]  s     Original string
 * @param[in]  from  String to be replaced by @p to
 * @param[in]  to    String that replaces @p from
 *
 * @return     String with replaced substrings.
 */
std::string replace_all(std::string_view const& s, std::string_view const& from,
                        std::string_view const& to);

inline void ensure(bool cond, std::string const& msg, int err = errno) {
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
