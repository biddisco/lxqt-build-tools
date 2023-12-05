#include "network/evp-encrypt.hpp"

int main(int argc, char* argv[])
{
  // plaintext, ciphertext, recovered text
  secure_string ptext = "Now is the time for all good men to come to the aide of their country";
  secure_string randbytes = "";

  encryption encyptor("a random piece of text used as a password", randbytes);

  auto encrypted = encyptor.encrypt(ptext);
  auto decrypted = encyptor.decrypt(encrypted);
  auto hashed = encyptor.CalcHmacSHA256("12345abcde54321", "the quick brown fox");
  std::cout << "Original message:\n" << ptext << std::endl;
  std::cout << "Recovered message:\n" << decrypted << std::endl;
  std::cout << "Hashed message:\n" << hashed << std::endl;

  return (ptext == decrypted) ? EXIT_SUCCESS : EXIT_FAILURE;
}
