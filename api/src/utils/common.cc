#include "utils/common.h"
using namespace utils;

static const char *HEX = "0123456789ABCDEF";
static const char *HEXL = "0123456789abcdef";
static const int BASE64_IDX[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  62, 63, 62, 62, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,
	0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
	19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63, 0,  26, 27, 28, 29, 30, 31, 32, 33,
	34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};
static const char *BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const uint32_t MOD_TABLE[] = {0, 2, 1};

static int from_hex_char(char c) {
	if ('0' <= c && c <= '9')
		return c - '0';
	if ('A' <= c && c <= 'F')
		return c - 'A' + 10;
	if ('a' <= c && c <= 'f')
		return c - 'a' + 10;
	return 0;
}

std::string utils::url_decode(const std::string_view &str) {
	std::string ret;
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] != '%' || i + 3 > str.size()) {
			if (str[i] == '+')
				ret += ' ';
			else
				ret += str[i];
		} else {
			unsigned int ii = from_hex_char(str[i + 1]) * 16 + from_hex_char(str[i + 2]);
			ret += (char) ii;
			i = i + 2;
		}
	}
	return ret;
}

std::string utils::url_encode(const std::string_view &str) {
	std::string ret;
	for (char c : str) {
		if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9')) {
			ret += c;
		} else {
			ret += '%';
			ret += HEX[(unsigned char) (c) / 16];
			ret += HEX[(unsigned char) (c) % 16];
		}
	}
	return ret;
}

parameters_map utils::parse_query_string(const std::string_view &qs) {
	parameters_map data;
	std::string key, value;
	bool on_key = true;
	for (char c : qs) {
		if (c == '&') {
			data[url_decode(key)] = url_decode(value);
			key.clear();
			value.clear();
			on_key = true;
		} else if (c == '=') {
			on_key = false;
		} else {
			(on_key ? key : value) += c;
		}
	}
	if (key != "" || value != "")
		data[url_decode(key)] = url_decode(value);
	return data;
}

parameters_map utils::parse_cookies(const char *s) {
	if (!s) {
		return {};
	}
	parameters_map data;
	std::string key, value;
	bool on_key = true;
	for (char c : std::string_view{s}) {
		if (c == ' ' || c == '"') {
			continue;
		}
		if (c == '=') {
			on_key = false;
		} else if (c == ';') {
			data[url_decode(key)] = url_decode(value);
			key.clear();
			value.clear();
			on_key = true;
		} else {
			(on_key ? key : value) += c;
		}
	}
	if (key != "" || value != "") {
		data[url_decode(key)] = url_decode(value);
	}
	return data;
}

std::string utils::b16_encode(const std::string_view &s) {
	std::string res(s.size() * 2, '0');
	for (std::size_t i = 0; i < s.size(); ++i) {
		res[2 * i] = HEXL[(unsigned char) (s[i]) / 16];
		res[2 * i + 1] = HEXL[(unsigned char) (s[i]) % 16];
	}
	return res;
}

std::string utils::b16_decode(const std::string_view &s) {
	std::string res(s.size() / 2, 0);
	for (std::size_t i = 0; i < s.size() / 2; ++i) {
		res[i] = char(from_hex_char(s[2 * i]) * 16 + from_hex_char(s[2 * i + 1]));
	}
	return res;
}

std::string utils::b64_decode(const std::string_view &s) {
	auto p = (const unsigned char *) s.data();
	size_t len = s.size();

	int pad = len > 0 && (len % 4 || p[len - 1] == '=');
	size_t L = ((len + 3) / 4 - pad) * 4;
	std::string str(L / 4 * 3 + pad, '\0');

	for (size_t i = 0, j = 0; i < L; i += 4) {
		int n = BASE64_IDX[p[i]] << 18 | BASE64_IDX[p[i + 1]] << 12 | BASE64_IDX[p[i + 2]] << 6 |
				BASE64_IDX[p[i + 3]];
		str[j++] = (char) (n >> 16);
		str[j++] = (char) (n >> 8 & 0xFF);
		str[j++] = (char) (n & 0xFF);
	}
	if (pad) {
		int n = BASE64_IDX[p[L]] << 18 | BASE64_IDX[p[L + 1]] << 12;
		str[str.size() - 1] = (char) (n >> 16);

		if (len > L + 2 && p[L + 2] != '=') {
			n |= BASE64_IDX[p[L + 2]] << 6;
			str.push_back((char) (n >> 8 & 0xFF));
		}
	}
	return str;
}

std::string utils::b64_encode(const std::string_view &s) {
	size_t len = s.size();
	auto data = (const unsigned char *) s.data();

	size_t sz = 4 * ((len + 2) / 3);
	std::string res(sz, 0);

	for (uint32_t i = 0, j = 0; i < len;) {
		uint32_t octet_a = i < len ? data[i++] : 0;
		uint32_t octet_b = i < len ? data[i++] : 0;
		uint32_t octet_c = i < len ? data[i++] : 0;
		uint32_t triple = (octet_a << 0x10) | (octet_b << 0x08) | octet_c;

		res[j++] = BASE64[(triple >> 3 * 6) & 0x3F];
		res[j++] = BASE64[(triple >> 2 * 6) & 0x3F];
		res[j++] = BASE64[(triple >> 1 * 6) & 0x3F];
		res[j++] = BASE64[(triple >> 0 * 6) & 0x3F];
	}

	for (uint32_t i = 0; i < MOD_TABLE[len % 3]; i++)
		res[sz - 1 - i] = '=';

	return res;
}

std::string utils::replace_all(const std::string_view &s, const std::string_view &from,
							   const std::string_view &to) {
	size_t start_pos = 0, prev_pos = 0;
	std::string result;
	while ((start_pos = s.find(from, prev_pos)) != std::string::npos) {
		result += s.substr(prev_pos, start_pos - prev_pos);
		result += to;
		prev_pos = start_pos + from.size();
	}
	result += s.substr(prev_pos);
	return result;
}