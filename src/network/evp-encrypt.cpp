#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
//
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
//
#include <curl/curl.h>
#include <uuid/uuid.h>
//
#include "evp-encrypt.hpp"

encryption::encryption(secure_string const& passphrase, secure_string& randbytes)
{
  // copy the passphrase into the key block until N chars are done
  // wrap around the passphrase if N>len(passphrase)
  std::generate_n(std::begin(key), encryption::KEY_SIZE,
      [n = 0, &passphrase]() mutable { return passphrase[n++ % passphrase.size()]; });

  if (randbytes.size() < (encryption::BLOCK_SIZE))
  {
    randbytes.resize(encryption::BLOCK_SIZE);
    int rc = RAND_bytes(iv, encryption::BLOCK_SIZE);
    if (rc != 1) throw std::runtime_error("RAND_bytes for iv failed");
    std::memcpy(&randbytes[0], iv, encryption::BLOCK_SIZE);
  }
  else { std::memcpy(iv, &randbytes[0], encryption::BLOCK_SIZE); }
}

encryption::~encryption()
{
  OPENSSL_cleanse(key, encryption::KEY_SIZE);
  OPENSSL_cleanse(iv, encryption::BLOCK_SIZE);
}

secure_string encryption::encrypt(secure_string const& input)
{
  secure_string result("");
  aes_encrypt(key, iv, input, result);
  return result;
}

secure_string encryption::decrypt(secure_string const& input)
{
  secure_string result("");
  aes_decrypt(key, iv, input, result);
  return result;
}

secure_string encryption::CalcHmacSHA256(secure_string const& key, secure_string const& msg)
{
  std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
  unsigned int hashLen;

  HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
      reinterpret_cast<unsigned char const*>(msg.data()), static_cast<int>(msg.size()), hash.data(),
      &hashLen);

  return {reinterpret_cast<char const*>(hash.data()), hashLen};
}

void aes_encrypt(byte const key[encryption::KEY_SIZE], byte const iv[encryption::BLOCK_SIZE],
    secure_string const& ptext, secure_string& ctext)
{
  EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
  if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

  int rc = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key, iv);
  if (rc != 1) throw std::runtime_error("EVP_EncryptInit_ex failed");

  // Recovered text expands upto encryption::BLOCK_SIZE
  ctext.resize(ptext.size() + encryption::BLOCK_SIZE);
  int out_len1 = (int) ctext.size();

  rc = EVP_EncryptUpdate(
      ctx.get(), (byte*) &ctext[0], &out_len1, (byte const*) &ptext[0], (int) ptext.size());
  if (rc != 1) throw std::runtime_error("EVP_EncryptUpdate failed");

  int out_len2 = (int) ctext.size() - out_len1;
  rc = EVP_EncryptFinal_ex(ctx.get(), (byte*) &ctext[0] + out_len1, &out_len2);
  if (rc != 1) throw std::runtime_error("EVP_EncryptFinal_ex failed");

  // Set cipher text size now that we know it
  ctext.resize(out_len1 + out_len2);
}

void aes_decrypt(byte const key[encryption::KEY_SIZE], byte const iv[encryption::BLOCK_SIZE],
    secure_string const& ctext, secure_string& rtext)
{
  EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
  if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

  int rc = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key, iv);
  if (rc != 1) throw std::runtime_error("EVP_DecryptInit_ex failed");

  // Recovered text contracts upto encryption::BLOCK_SIZE
  rtext.resize(ctext.size());
  int out_len1 = (int) rtext.size();

  rc = EVP_DecryptUpdate(
      ctx.get(), (byte*) &rtext[0], &out_len1, (byte const*) &ctext[0], (int) ctext.size());
  if (rc != 1) throw std::runtime_error("EVP_DecryptUpdate failed");

  int out_len2 = (int) rtext.size() - out_len1;
  rc = EVP_DecryptFinal_ex(ctx.get(), (byte*) &rtext[0] + out_len1, &out_len2);
  if (rc != 1) throw std::runtime_error("EVP_DecryptFinal_ex failed");

  // Set recovered text size now that we know it
  rtext.resize(out_len1 + out_len2);
}

std::string b2a_hex(unsigned char const* byte_arr, int n)
{
  static std::string const hex_codes = "0123456789abcdef";
  std::string hex_string;
  for (int i = 0; i < n; ++i)
  {
    unsigned char bin_value = byte_arr[i];
    hex_string += hex_codes[(bin_value >> 4) & 0x0F];
    hex_string += hex_codes[bin_value & 0x0F];
  }
  return hex_string;
}

std::string encryption::generate_uuid_string()
{
#ifdef GROX_HAVE_UUID_ENCODING
  using uuid_string_t = char[256];
  // from https://www.bitstamp.net/api/ on using api with token authorization
  uuid_t uuid;
  uuid_string_t nonce;
  uuid_generate(uuid);
  uuid_unparse_lower(uuid, nonce);
  return nonce;
#else
  throw std::runtime_error("GROX_HAVE_UUID_ENCODING not set in call to generate_uuid_string")
#endif
}

std::string generate_random_alphanumeric_string(std::size_t len, std::uint64_t seed)
{
  static constexpr auto chars = "0123456789"
                                "~`!@#$%^&*()_-+={}[]|';:/?<>,."
                                "!@#$%^&*(){}][:;'/?.>,<'`~| "
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz";
  auto rng = std::mt19937(seed);
  auto dist = std::uniform_int_distribution{{}, std::strlen(chars) - 1};
  auto result = std::string(len, '\0');
  std::generate_n(begin(result), len, [&]() { return chars[dist(rng)]; });
  return result;
}

std::string url_encode(std::string_view value)
{
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (std::string_view::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
  {
    std::string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
    {
      escaped << c;
      continue;
    }

    // Any other characters are percent-encoded
    escaped << std::uppercase;
    escaped << '%' << std::setw(2) << int((unsigned char) c);
    escaped << std::nouppercase;
  }

  return escaped.str();
}
