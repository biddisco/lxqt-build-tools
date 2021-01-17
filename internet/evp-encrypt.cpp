#include <iostream>
#include <string>
#include <memory>
#include <limits>
#include <stdexcept>
#include <cstring>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "evp-encrypt.hpp"

encryption::encryption(const secure_string &passphrase, secure_string &randbytes)
{
    passphrase_ = passphrase.data();
    if (randbytes.size() < (encryption::KEY_SIZE + encryption::BLOCK_SIZE)) {
        gen_params(key, iv);
        randbytes.resize(encryption::KEY_SIZE + encryption::BLOCK_SIZE);
        std::memcpy(&randbytes[0],                    key, encryption::KEY_SIZE);
        std::memcpy(&randbytes[encryption::KEY_SIZE], iv,  encryption::BLOCK_SIZE);
    }
    else {
        std::memcpy(key, &randbytes[0],                    encryption::KEY_SIZE);
        std::memcpy(iv,  &randbytes[encryption::KEY_SIZE], encryption::BLOCK_SIZE);
    }
}

encryption::~encryption() {
    OPENSSL_cleanse(key, encryption::KEY_SIZE);
    OPENSSL_cleanse(iv, encryption::BLOCK_SIZE);
}

secure_string encryption::encrypt(const secure_string &input) {
    secure_string result("");
    aes_encrypt(key, iv, input, result);
    return result;
}

secure_string encryption::decrypt(const secure_string &input) {
    secure_string result("");
    aes_decrypt(key, iv, input, result);
    return result;
}

void gen_params(byte key[encryption::KEY_SIZE], byte iv[encryption::BLOCK_SIZE])
{
    int rc = RAND_bytes(key, encryption::KEY_SIZE);
    if (rc != 1)
      throw std::runtime_error("RAND_bytes key failed");

    rc = RAND_bytes(iv, encryption::BLOCK_SIZE);
    if (rc != 1)
      throw std::runtime_error("RAND_bytes for iv failed");
}

void aes_encrypt(const byte key[encryption::KEY_SIZE], const byte iv[encryption::BLOCK_SIZE], const secure_string& ptext, secure_string& ctext)
{
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    int rc = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), NULL, key, iv);
    if (rc != 1)
      throw std::runtime_error("EVP_EncryptInit_ex failed");

    // Recovered text expands upto encryption::BLOCK_SIZE
    ctext.resize(ptext.size()+encryption::BLOCK_SIZE);
    int out_len1 = (int)ctext.size();

    rc = EVP_EncryptUpdate(ctx.get(), (byte*)&ctext[0], &out_len1, (const byte*)&ptext[0], (int)ptext.size());
    if (rc != 1)
      throw std::runtime_error("EVP_EncryptUpdate failed");
  
    int out_len2 = (int)ctext.size() - out_len1;
    rc = EVP_EncryptFinal_ex(ctx.get(), (byte*)&ctext[0]+out_len1, &out_len2);
    if (rc != 1)
      throw std::runtime_error("EVP_EncryptFinal_ex failed");

    // Set cipher text size now that we know it
    ctext.resize(out_len1 + out_len2);
}

void aes_decrypt(const byte key[encryption::KEY_SIZE], const byte iv[encryption::BLOCK_SIZE], const secure_string& ctext, secure_string& rtext)
{
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    int rc = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), NULL, key, iv);
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptInit_ex failed");

    // Recovered text contracts upto encryption::BLOCK_SIZE
    rtext.resize(ctext.size());
    int out_len1 = (int)rtext.size();

    rc = EVP_DecryptUpdate(ctx.get(), (byte*)&rtext[0], &out_len1, (const byte*)&ctext[0], (int)ctext.size());
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptUpdate failed");
  
    int out_len2 = (int)rtext.size() - out_len1;
    rc = EVP_DecryptFinal_ex(ctx.get(), (byte*)&rtext[0]+out_len1, &out_len2);
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptFinal_ex failed");

    // Set recovered text size now that we know it
    rtext.resize(out_len1 + out_len2);
}

