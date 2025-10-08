#include <iostream>
#include <AuthorKey.hh>

int main(void)
{
    std::cout << Yps::AuthorKey::getInstance().get_author_id() << std::endl << std::endl;
    for (uint32_t i = 0; i < Yps::AuthorKey::getInstance().get_key().size(); ++i)
    {
        std::cout << static_cast<uint16_t>(Yps::AuthorKey::getInstance().get_key()[i]);
    }
    std::cout << std::endl << std::endl;

    std::cout << "Type of seed: " << Yps::AuthorKey::getInstance().get_id_type() << std::endl;
    return 0;
}