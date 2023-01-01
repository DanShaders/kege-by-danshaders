#include "utils/crypto.h"
#include "config.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include "utils/api.h"
#include "utils/common.h"
using namespace utils;

std::string utils::hmac_sign(std::string_view const& value, std::string_view const& key) {
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned int result_len;
  HMAC(EVP_sha384(), (unsigned char const*) key.data(), (int) key.size(),
       (unsigned char const*) value.data(), (int) value.size(), result, &result_len);
  return utils::b64_encode(std::string_view((char*) result, result_len));
}

std::string utils::sha3_256(std::string_view const& data) {
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned int result_len = 0;

  EVP_MD_CTX* context = EVP_MD_CTX_new();
  EVP_DigestInit_ex(context, EVP_sha3_256(), nullptr);
  EVP_DigestUpdate(context, data.data(), data.size());
  EVP_DigestFinal_ex(context, result, &result_len);
  EVP_MD_CTX_destroy(context);

  return std::string{result, result + result_len};
}

static std::string const& get_sign_key(bool create = true) {
  static std::string key;
  if (create && key.empty()) {
    key = urandom(32);
  }
  return key;
}

static int64_t get_current_time() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string utils::sign_url(
    std::string_view const& url,
    std::vector<std::pair<std::string_view, std::string_view>> const& params, bool full) {
  std::ostringstream oss_params;
  bool is_first = true;
  auto append = [&](std::string_view key, std::string_view value) {
    oss_params << (is_first ? "" : "&") << key << "=" << url_encode(value);
    is_first = false;
  };

  for (auto [key, value] : params) {
    append(key, value);
  }
  if (full) {
    append("stime", std::to_string(get_current_time()));
    append("svalue", b64_encode(hmac_sign(oss_params.str(), get_sign_key())));
  } else {
    auto stime = std::to_string(get_current_time()), srandom = b64_encode(urandom(18));
    append("stime", stime);
    append("srandom", srandom);
    append("svalue", b64_encode(hmac_sign(stime + "&" + srandom, get_sign_key())));
  }

  std::ostringstream oss_url;
  oss_url << conf.api_root << url << "?" << oss_params.str();
  return oss_url.str();
}

static const int64_t VALID_TIME = 10 * 3'600'000;  // 10 hours

bool utils::is_signed(fcgx::request_t* r, bool full) noexcept {
  if (auto it = r->params.find("stime"); it == r->params.end()) {
    return false;
  } else {
    auto const& stime = it->second;
    if (stime.size() >= 18) {
      return false;
    }
    for (char c : stime) {
      if (!isdigit(c)) {
        return false;
      }
    }
    if (abs(stoll(stime) - get_current_time()) > VALID_TIME) {
      return false;
    }
  }
  if (auto it = r->params.find("svalue"); it == r->params.end()) {
    return false;
  } else {
    auto const &to_check = it->second, &s = r->raw_params, &key = get_sign_key(false);

    if (key.empty()) {
      return false;
    }

    if (full) {
      std::size_t pos = s.npos;
      for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '?' || s[i] == '&') {
          pos = i;
        }
      }
      if (pos == s.npos) {
        return false;
      }
      if (b64_encode(hmac_sign({s.begin(), s.begin() + pos}, key)) != to_check) {
        return false;
      }
    } else {
      auto it1 = r->params.find("stime");
      auto it2 = r->params.find("srandom");
      if (it1 == r->params.end() || it2 == r->params.end()) {
        return false;
      }
      if (b64_encode(hmac_sign(it1->second + "&" + it2->second, key)) != to_check) {
        return false;
      }
    }
  }
  return true;
}

void utils::signature_required(fcgx::request_t* r, bool full) {
  if (!is_signed(r, full)) {
    r->headers.push_back("status: 403");
    throw expected_error();
  }
}

std::string utils::urandom(int len) {
  std::string res(len, 0);
  if (RAND_bytes((unsigned char*) res.data(), len) != 1) {
    throw std::runtime_error("RAND_bytes failed");
  }
  return res;
}

std::string utils::urandom_priv(int len) {
  std::string res(len, 0);
  if (RAND_priv_bytes((unsigned char*) res.data(), len) != 1) {
    throw std::runtime_error("RAND_priv_bytes failed");
  }
  return res;
}