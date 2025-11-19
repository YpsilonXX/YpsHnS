#ifndef YPSHNS_EMBEDDATA_HH
#define YPSHNS_EMBEDDATA_HH

#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <defines.hh>
#include "Encryption.hh"

namespace Yps
{
    enum class ContainerType : uint8_t
    {
        UNKNOWN, PHOTO, VIDEO, AUDIO
    };

    enum class Extension : uint8_t
    {
        JPEG, PNG
    };

    enum class LsbMode : uint8_t
    {
        OneBit,
        TwoBits,
        NoUsed
    };

    // Fully POD-compatible structure — safe for memcpy
    struct MetaData
    {
        ContainerType container = ContainerType::UNKNOWN;
        Extension     ext       = Extension::JPEG;
        uint64_t      write_size = 0;           // Total size: metadata + encrypted payload
        LsbMode       lsb_mode   = LsbMode::NoUsed;
        uint32_t      meta_size  = sizeof(MetaData);  // Fixed size

        // Fixed-size buffer for embedded file name (supports long paths, UTF-8)
        char          filename[1024] = {0};
    };

    struct EmbedData
    {
        std::vector<byte> plain_data;
        std::vector<byte> encrypt_data;
        MetaData          meta;
        uint64_t          max_capacity{};
        std::array<byte, SHA256_DIGEST_LENGTH> key;
    };

} // namespace Yps

#endif // YPSHNS_EMBEDDATA_HH