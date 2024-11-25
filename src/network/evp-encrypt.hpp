#pragma once

#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <openssl/evp.h>
#include <openssl/rand.h>

// ------------------------------------------------------------------
// utility functions
// ------------------------------------------------------------------
// generate random chars with length, seed
std::string generate_random_alphanumeric_string(std::size_t /*len*/, std::uint64_t /*seed*/);

// encode a URL by escaping necessary chars
std::string url_encode(std::string_view);
std::string b2a_hex(std::string_view /*byte_arr*/, int /*n*/);

// ------------------------------------------------------------------
// allocator that cleans up memory
// ------------------------------------------------------------------
template <typename T>
struct zallocator
{
  public:
  typedef T value_type;
  typedef value_type* pointer;
  typedef value_type const* const_pointer;
  typedef value_type& reference;
  typedef value_type const& const_reference;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  pointer address(reference v) const { return &v; }
  const_pointer address(const_reference v) const { return &v; }

  pointer allocate(size_type n, void const* hint = 0)
  {
    if (n > std::numeric_limits<size_type>::max() / sizeof(T)) throw std::bad_alloc();
    return static_cast<pointer>(::operator new(n * sizeof(value_type)));
  }

  void deallocate(pointer p, size_type n)
  {
    OPENSSL_cleanse(p, n * sizeof(T));
    ::operator delete(p);
  }

  size_type max_size() const { return std::numeric_limits<size_type>::max() / sizeof(T); }

  template <typename U>
  struct rebind
  {
    typedef zallocator<U> other;
  };

  void construct(pointer ptr, T const& val) { new (static_cast<T*>(ptr)) T(val); }

  void destroy(pointer ptr) { static_cast<T*>(ptr)->~T(); }

  template <typename U, typename... Args>
  void construct(U* ptr, Args&&... args)
  {
    ::new (static_cast<void*>(ptr)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* ptr)
  {
    ptr->~U();
  }
};

using byte = unsigned char;

//typedef std::basic_string<char, std::char_traits<char>, zallocator<char> > secure_string;
using secure_string = std::string;
using EVP_CIPHER_CTX_free_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&::EVP_CIPHER_CTX_free)>;

void gen_params(byte* key, byte* iv);
void aes_encrypt(
    byte const* key, byte const* iv, secure_string const& instring, secure_string& outstring);
void aes_decrypt(
    byte const* key, byte const* iv, secure_string const& instring, secure_string& outstring);

struct encryption
{
  static unsigned int const KEY_SIZE = 32;
  static unsigned int const BLOCK_SIZE = 16;
  //
  byte key[KEY_SIZE];
  byte iv[BLOCK_SIZE];
  //
  encryption(secure_string const& passphrase, secure_string& randbytes);
  ~encryption();

  // core encryption routines
  secure_string encrypt(secure_string const& input);
  secure_string decrypt(secure_string const& input);

  // utility function for signing web request
  static secure_string CalcHmacSHA256(secure_string const& decodedKey, secure_string const& msg);

  std::string generate_uuid_string();
};
