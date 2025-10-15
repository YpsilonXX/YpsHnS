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


    std::optional<std::vector<byte>> PhotoHnS::extract(const std::string& path)
    {
        std::optional<std::vector<byte>> data;
        return data;
    }


} // Yps