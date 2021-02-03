#pragma once

#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <openssl/evp.h>
#include <openssl/rand.h>

template <typename T>
struct zallocator
{
public:
    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    pointer address(reference v) const
    {
        return &v;
    }
    const_pointer address(const_reference v) const
    {
        return &v;
    }

    pointer allocate(size_type n, const void* hint = 0)
    {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T))
            throw std::bad_alloc();
        return static_cast<pointer>(::operator new(n * sizeof(value_type)));
    }

    void deallocate(pointer p, size_type n)
    {
        OPENSSL_cleanse(p, n * sizeof(T));
        ::operator delete(p);
    }

    size_type max_size() const
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    template <typename U>
    struct rebind
    {
        typedef zallocator<U> other;
    };

    void construct(pointer ptr, const T& val)
    {
        new (static_cast<T*>(ptr)) T(val);
    }

    void destroy(pointer ptr)
    {
        static_cast<T*>(ptr)->~T();
    }

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
using EVP_CIPHER_CTX_free_ptr =
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&::EVP_CIPHER_CTX_free)>;

void gen_params(byte* key, byte* iv);
void aes_encrypt(const byte* key, const byte* iv, const secure_string& instring,
    secure_string& outstring);
void aes_decrypt(const byte* key, const byte* iv, const secure_string& instring,
    secure_string& outstring);

struct encryption
{
    static const unsigned int KEY_SIZE = 32;
    static const unsigned int BLOCK_SIZE = 16;
    //
    byte key[KEY_SIZE];
    byte iv[BLOCK_SIZE];
    //
    encryption(const secure_string& passphrase, secure_string& randbytes);
    ~encryption();

    // core encryption routines
    secure_string encrypt(const secure_string& input);
    secure_string decrypt(const secure_string& input);

    // utility function for signing web request
    secure_string CalcHmacSHA256(
        const secure_string& decodedKey, const secure_string& msg);

#ifdef GROX_HAVE_CURL_ENCODING
    // utility functions for URL encoding non asci chars
    std::string b2a_hex(char* byte_arr, int n);
    std::string url_encode(std::string data);
#endif

#ifdef GROX_HAVE_UUID_ENCODING
    std::string generate_uuid_string();
#endif
};
