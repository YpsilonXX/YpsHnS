//
// Created by ypsilon on 10/7/25.
//

#ifndef YPSHNS_EMBEDDATA_HH
#define YPSHNS_EMBEDDATA_HH

#include <vector>
#include <string>
#include <array>
#include <optional>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <defines.hh>
#include <openssl/sha.h>

namespace Yps
{
    enum class ContainerType
    {
        UNKNOWN, PHOTO, VIDEO, AUDIO
    };

    enum class Extension
    {
        JPEG, PNG
    };

    enum class LsbMode {
        OneBit,
        TwoBits,
        NoUsed
    };

    struct MetaData
    {
        /**
         * Type of container
         */
        ContainerType container;

        /**
         * Container's extension
         */
        Extension ext;

        /**
         * Name of plain(to embed) file
         */
        std::string filename;

        /**
         * Size of all written path (meta + plain)
         */
        uint64_t write_size;

        /**
         * Type of written lsb_mode
         */
        LsbMode lsb_mode{LsbMode::NoUsed};

        /**
         * Size of meta_data
         */
        const uint32_t meta_size = sizeof(MetaData);
    };


    struct EmbedData
    {
        EmbedData() = default;
        ~EmbedData() = default;

        /**
         * Raw Data
         */
        std::vector<byte> plain_data;
        /**
         * Encrypted Data
         */
        std::vector<byte> encrypt_data;

        /**
         * Metadata to embed with plain data
         */
        MetaData meta;

        /**
         * Max size of data to embed
         */
        uint64_t max_capacity{};

        /**
         * Key for cryptography
         */
        std::array<byte, SHA256_DIGEST_LENGTH> key;
    };

}

#endif //YPSHNS_EMBEDDATA_HH