//
// Created by ypsilon on 10/7/25.
//

#ifndef YPSHNS_EMBEDDATA_HH
#define YPSHNS_EMBEDDATA_HH

#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <defines.hh>

namespace Yps
{

struct EmbedData
{
    EmbedData() = default;
    explicit EmbedData(std::vector<std::uint8_t> plain_data)
        : plaintext(std::move(plain_data)) {}
    ~EmbedData() { secure_erase_key(); }


    /**
     * Source data for encrypt and embed
     * @note data stored as vector of bytes for validity all types
     */
    std::vector<byte> plain_data;

    /**
     * Data ready to embed to file
     */
    std::vector<byte> embed_data;

    std::optional<std::vector<uint8_t>> encryption_key;
    std::optional<std::vector<uint8_t>> init_vector;

    struct MediaMetadata
    {
        /**type: "image/jpeg", "video/mp4", etc*/
        std::string type;
        std::size_t embed_position;
        std::size_t max_capacity;   //for embed
    };
    std::optional<MediaMetadata> metadata;

    enum class Status
    {
        Uninitialized,
        Encrypted,
        Embedded,
        Extracted
    };
    Status status = Status::Uninitialized;

private:
    /**
     * Secure delete encryption_key
     */
    void secure_erase_key() {
        if (encryption_key) {
            std::fill(encryption_key->begin(), encryption_key->end(), 0);
            encryption_key->clear();
            encryption_key.reset();
        }
    }
};

}

#endif //YPSHNS_EMBEDDATA_HH