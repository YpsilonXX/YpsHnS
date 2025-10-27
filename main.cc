#include <iostream>
#include <AuthorKey.hh>
#include <cstdlib>

#include "EmbedData.hh"
#include "Encryption.hh"
#include "PhotoHnS/PhotoHnS.hh"

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

    std::string line = "Это зашифрованный текст +-=*/аааа";
    std::cout << line << std::endl;

    std::vector<byte> data(line.c_str(), line.c_str() + line.size());

    Yps::PhotoHnS ph;
    ph.embed(data, "test.png", "out.png");
    std::optional<std::vector<byte>> d_out = ph.extract("out.png");
    if (d_out.has_value())
    {
        if (data.size() != d_out.value().size())
            std::cout << "size mismatch" << std::endl;
        for (int i = 0; i < data.size(); i++)
        {
            if (data[i] != d_out.value()[i])
                std::cout << "data mismatch" << std::endl;
        }
    }


    return 0;
}