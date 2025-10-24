#include "PhotoHnS.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>

namespace Yps
{
    bool PhotoHnS::has_usable_alpha(const byte *image, int32_t width, int32_t height, int32_t channels)
    {
        if (channels != 4)
            return false;
        for (uint32_t i = 3; i < width * height * channels; i += channels)
            if (image[i] != 255)
                return false;
        return true;
    }


    std::optional<std::string> PhotoHnS::embed(const std::vector<byte>& data, const std::string& path, const std::string& out_path)
    {
        /*Check embed_data and init*/
        if (this->embed_data == nullptr)
            this->embed_data = std::make_unique<EmbedData>();
        else
        {
            this->embed_data.release();
            this->embed_data = std::make_unique<EmbedData>();
        }

        /*Copy data to embed_data*/
        this->embed_data->plain_data = data;
        this->embed_data->meta.container = ContainerType::PHOTO;
        this->embed_data->meta.filename = path;
        this->embed_data->key = AuthorKey::getInstance().get_key();

        /*Encrypt data*/
        AES256Encryption::getInstance().set_key(this->embed_data->key);
        this->embed_data->encrypt_data = AES256Encryption::getInstance().encrypt(this->embed_data->plain_data);
        this->embed_data->meta.write_size = this->embed_data->encrypt_data.size() + this->embed_data->meta.meta_size;

        /*Check type*/
        std::string filetype;
        if (this->validate_path(path).has_value())
            filetype = validate_path(path).value();
        else
        {
            std::cerr << CLI_RED << "PhotoHnS::PhotoHnS(): Invalid path" << std::endl;
            return std::nullopt;
        }

        /*switch type*/
        if (filetype == "png")
        {
            this->embed_data->meta.ext = Extension::PNG;
            /*Надо как-то обработать результат*/
            return this->png_in(out_path);
        }

        return "Empty";
    }

    std::optional<std::string> PhotoHnS::png_in(const std::string &out_path)
    {
        /*load image*/
        int32_t width, height, channels;
        byte* image = stbi_load(this->embed_data->meta.filename.c_str(), &width, &height, &channels, 0);
        if (!image)
        {
            std::cout << CLI_RED << "Error: stbi_load return nullptr. PNG load ended with error." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        /*Check available bits*/
        const uint64_t bits_OneBit = (width * height * channels);
        const uint64_t total_bits = (this->embed_data->encrypt_data.size() + this->embed_data->meta.meta_size) * 8;
        if (total_bits < bits_OneBit)
                this->embed_data->meta.lsb_mode = LsbMode::OneBit;
        else if (total_bits < bits_OneBit * 2 - this->embed_data->meta.meta_size * 8)
        {
            std::cout << CLI_YELLOW << "Warning: using LsbMode::TwoBits. Collisions may be visible" << CLI_RESET << std::endl;
            this->embed_data->meta.lsb_mode = LsbMode::TwoBits;
        }
        else
        {
            std::cout << CLI_RED << "Error: available bits in PNG picture is not enough" << CLI_RESET << std::endl;
            return std::nullopt;
        }

        /*Prepare data to embed*/
        std::vector<byte> full_data(this->embed_data->meta.write_size);
        MetaData* m = &this->embed_data->meta;
        std::copy((byte*)m, (byte*)(m) + sizeof(MetaData), full_data.begin());
        std::copy(this->embed_data->encrypt_data.begin(), this->embed_data->encrypt_data.end(), full_data.begin() + sizeof(MetaData));

        /*Main process*/
        switch (this->embed_data->meta.lsb_mode)
        {
            case LsbMode::OneBit:
                this->lsb_one_bit(image, full_data);
                break;
            case LsbMode::TwoBits:
                this->lsb_two_bit(image, full_data);
                break;

            default:
                std::cout << CLI_RED << "Error: LsbMode type error" << CLI_RESET << std::endl;
                return std::nullopt;
        }

        int result = stbi_write_png(out_path.c_str(), width, height, channels, image, 0);
        if (result == 0)
            std::cout << CLI_RED << "Error: stbi_write_png failed" << CLI_RESET << std::endl;
        stbi_image_free(image);

        std::optional<std::string> out = out_path;

        return result ? out : std::nullopt;
    }

    void PhotoHnS::lsb_one_bit(byte *image, std::vector<byte>& data)
    {
        const uint64_t total_bits = data.size() * 8;
        uint64_t bit_index = 0;

        for (uint64_t i = 0; bit_index < total_bits && i < total_bits; ++i)
        {
            /*current byte and bit to embed*/
            const byte current_byte = data[bit_index / 8];
            const byte bit = (current_byte >> (7 - (bit_index % 8))) & 1;

            /*Edit bit*/
            image[i] = (image[i]) & 0xFE | bit;

            /*go to the next bit*/
            ++bit_index;
        }

    }

    /*Need to check and correct*/
    void PhotoHnS::lsb_two_bit(byte *image, std::vector<byte> &data)
    {
        const uint64_t total_bits = data.size() * 8;
        const uint64_t meta_bits = this->embed_data->meta.meta_size * 8;
        uint64_t bit_index = 0;
        uint64_t image_index = 0;

        /*Embed meta as one_bit mode*/
        for (; bit_index < meta_bits && image_index < total_bits; ++image_index)
        {
            /*current byte and bit to embed*/
            const byte current_byte = data[bit_index / 8];
            const byte bit = (current_byte >> (7 - (bit_index % 8))) & 1;

            /*Edit bit*/
            image[image_index] = (image[image_index]) & 0xFE | bit;

            /*go to the next bit*/
            ++bit_index;
        }

        /*Embed remaining data*/
        for (; bit_index < total_bits && image_index < total_bits; ++image_index)
        {
            /*current byte and bits to embed*/
            const byte current_byte = data[bit_index / 8];
            const byte bits = (current_byte >> (6 - (bit_index % 8))) & 0x03;

            /*Edit bits*/
            image[image_index] = (image[image_index]) & 0xFC | bits;

            /*go to next bits*/
            bit_index += 2;
        }

    }

    std::optional<std::string> PhotoHnS::png_out(byte* image, uint64_t img_size, MetaData& meta, const std::string& path)
    {
        // Prepare full_data (meta + encrypt_data)
        std::vector<byte> full_data(meta.write_size, 0);
        std::copy(reinterpret_cast<const byte*>(&meta), reinterpret_cast<const byte*>(&meta) + meta_bytes_size,
              full_data.begin());

        // Extract encrypt_data based on lsb_mode (starting after meta)
        uint64_t encrypt_bits_start = meta.meta_size * 8;
        uint64_t encrypt_bytes_size = meta.write_size - meta.meta_size;
        uint64_t encrypt_bits = encrypt_bytes_size * 8;
        uint64_t bit_pos = encrypt_bits_start;  // Start after meta bits
        uint64_t img_idx = meta.meta_size;  // Image bytes used for meta (1 bit/byte)

        bool extraction_success = true;
        if (meta.lsb_mode == LsbMode::OneBit) {
            // OneBit: 1 bit per image byte
            for (; bit_pos < total_bits && img_idx < num_image_bytes; ++img_idx, ++bit_pos) {
                byte bit = image[img_idx] & 0x01;
                uint64_t byte_idx = bit_pos / 8 + meta_bytes_size;  // Offset to encrypt_data
                uint64_t bit_offset = bit_pos % 8;
                full_data[byte_idx] |= (bit << (7 - bit_offset));  // MSB-first
            }
        } else if (meta.lsb_mode == LsbMode::TwoBits) {
            // TwoBits: 2 bits per image byte (mirror embed: MSB-first shift)
            for (; bit_pos < total_bits && img_idx < num_image_bytes; img_idx++, bit_pos += 2) {
                if (bit_pos + 1 >= total_bits) break;  // Ensure even bits
                byte bits = image[img_idx] & 0x03;  // Lower 2 bits
                uint64_t byte_idx = bit_pos / 8 + meta_bytes_size;
                uint64_t bit_offset = bit_pos % 8;
                uint64_t shift = 6 - bit_offset;  // 6,4,2,0 for %8=0,2,4,6 (matches lsb_two_bit)
                full_data[byte_idx] |= (bits << shift);
            }
        } else {
            extraction_success = false;
            std::cout << CLI_RED << "Error: Unsupported LsbMode: " << static_cast<int>(meta.lsb_mode) << CLI_RESET << std::endl;
        }

        if (!extraction_success || bit_pos < total_bits) {
            std::cout << CLI_RED << "Error: Incomplete data extraction (mode: "
                      << static_cast<int>(meta.lsb_mode) << ", extracted: " << bit_pos << "/" << total_bits << " bits)" << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Extract encrypt_data
        std::vector<byte> encrypt_data(full_data.begin() + meta_bytes_size, full_data.end());
        embed_data->encrypt_data = encrypt_data;


        // Decrypt (use class-wide key logic)
        std::array<byte, SHA256_DIGEST_LENGTH> key = this->embed_data->key;
        AES256Encryption::getInstance().set_key(key);
        std::vector<byte> plain_data = AES256Encryption::getInstance().decrypt(encrypt_data);
        embed_data->plain_data = plain_data;

        std::cout << CLI_GREEN << "Success: Extracted " << plain_data.size() << " plain bytes (mode: "
                  << static_cast<int>(meta.lsb_mode) << ")" << CLI_RESET << std::endl;

        return path;
    }

    std::optional<std::vector<byte>> PhotoHnS::extract(const std::string& path)
    {
        /*Load image*/
        int32_t width, height, channels;
        byte* image = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!image) {
            std::cout << CLI_RED << "Error: Failed to load PNG from " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }

        /*Size of metainfo in bits*/
        uint64_t meta_size = sizeof(MetaData) * 8;
        if (meta_size > width * height * channels * 8)
        {
            stbi_image_free(image);
            std::cout << CLI_RED << "Error: size of file too small" << path << CLI_RESET << std::endl;
            return std::nullopt;
        }

        std::vector<byte> meta_bytes(meta_size, 0);
        size_t bit_pos = 0;
        for (size_t i = 0; i < sizeof(MetaData) * 8; ++i) {
            byte bit = image[i] & 0x01;  // LSB bit
            size_t byte_idx = bit_pos / 8;
            size_t bit_offset = bit_pos % 8;
            meta_bytes[byte_idx] |= (bit << (7 - bit_offset));  // set current bit
            ++bit_pos;
        }

        /*Parse Meta*/
        std::copy(meta_bytes.begin(), meta_bytes.end(), reinterpret_cast<byte*>(&this->embed_data->meta));

        /*Check Meta for valid*/
        if (this->embed_data->meta.container != ContainerType::PHOTO) {
            stbi_image_free(image);
            std::cout << CLI_RED << "Error: Invalid metadata in image" << CLI_RESET << std::endl;
            return std::nullopt;
        }

        switch (this->embed_data->meta.ext)
        {
            case Extension::PNG:
                png_out(path);
                break;
            case Extension::JPEG:
                break;
            default:
                break;
        }

        std::optional<std::vector<byte>> data = this->embed_data->plain_data;
        return data;
    }


} // Yps