// EmbedData.hh
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
         * Name of plain(to embed) file (fixed-size to ensure POD for safe memcpy)
         */
        char filename[256];

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
        uint32_t meta_size;
    };


}

#endif //YPSHNS_EMBEDDATA_HH