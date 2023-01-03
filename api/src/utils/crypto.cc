#include "utils/crypto.h"
#include "config.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include "utils/api.h"
#include "utils/common.h"
using namespace utils;

using uchar = unsigned char;

signature_t utils::hmac_sign(std::string_view value, std::string_view key) {
  unsigned result_len;
  signature_t result;

  assert(key.size() <= std::numeric_limits<int>::max());
  HMAC(EVP_sha3_256(), reinterpret_cast<uchar const*>(key.data()), static_cast<int>(key.size()),
       reinterpret_cast<uchar const*>(value.data()), value.size(),
       reinterpret_cast<uchar*>(result.begin()), &result_len);
  assert(result_len == signature_length);

  return result;
}

std::string utils::sha3_256(std::string_view data) {
  static constexpr size_t hash_length = 32;

  unsigned result_len;
  std::string result(hash_length, 0);

  EVP_MD_CTX* context = EVP_MD_CTX_new();
  EVP_DigestInit_ex(context, EVP_sha3_256(), nullptr);
  EVP_DigestUpdate(context, data.data(), data.size());
  EVP_DigestFinal_ex(context, reinterpret_cast<uchar*>(result.data()), &result_len);
  EVP_MD_CTX_destroy(context);
  assert(result_len == hash_length);

  return result;
}

std::string utils::urandom(int length) {
  std::string result(length, 0);
  if (RAND_bytes(reinterpret_cast<uchar*>(result.data()), length) != 1) {
    throw std::runtime_error("RAND_bytes failed");
  }
  return result;
}

std::string utils::urandom_priv(int length) {
  std::string result(length, 0);
  if (RAND_priv_bytes(reinterpret_cast<uchar*>(result.data()), length) != 1) {
    throw std::runtime_error("RAND_priv_bytes failed");
  }
  return result;
}