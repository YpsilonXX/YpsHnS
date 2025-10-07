//
// Created by ypsilon on 10/7/25.
//

#include "Encryption.hh"

namespace Yps
{
    std::vector<byte> Encryption::encrypt(const std::vector<byte>& data)
    {
        return data;
    }

    std::vector<byte> Encryption::decrypt(const std::vector<byte>& data)
    {
        return data;
    }

    Encryption &Encryption::getInstance()
    {
        static Encryption instance;
        return instance;
    }

} // Yps