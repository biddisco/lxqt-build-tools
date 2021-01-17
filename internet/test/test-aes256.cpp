#include <internet/evp-encrypt.hpp>

int main(int argc, char* argv[])
{
    // plaintext, ciphertext, recovered text
    secure_string ptext = "Now is the time for all good men to come to the aide of their country";
    secure_string randbytes = "";

    encryption encyptor("a random piece of text used as a password", randbytes);

    auto encrypted = encyptor.encrypt(ptext);
    auto decrypted = encyptor.decrypt(encrypted);
    std::cout << "Original message:\n" << ptext << std::endl;
    std::cout << "Recovered message:\n" << decrypted << std::endl;

    return 0;
}
