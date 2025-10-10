#include <iostream>
#include <AuthorKey.hh>

#include "Encryption.hh"

int main(void)
{
    std::cout << Yps::AuthorKey::getInstance().get_author_id() << std::endl << std::endl;
    for (uint32_t i = 0; i < Yps::AuthorKey::getInstance().get_key().size(); ++i)
    {
        std::cout << static_cast<uint16_t>(Yps::AuthorKey::getInstance().get_key()[i]);
    }
    std::cout << std::endl << std::endl;

    std::cout << "Type of seed: " << Yps::AuthorKey::getInstance().get_id_type() << std::endl;


    std::cout << "-------------------" << std::endl;

    std::string line = "Это зашифрованный текст";
    std::cout << line << std::endl;

    std::vector<byte> data(line.c_str(), line.c_str() + line.size());

    std::vector<byte> en_data = Yps::AES256Encryption::getInstance().encrypt(data);
    std::vector<byte> de_data = Yps::AES256Encryption::getInstance().decrypt(en_data);

    char* en_line = (char*)en_data.data();

    char* de_line = (char*)de_data.data();

    std::cout << en_line << std::endl;
    std::cout << de_line << std::endl;

    return 0;
}
