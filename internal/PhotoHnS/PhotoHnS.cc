#include "PhotoHnS.hh"

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
            this->png_in(out_path);
        }

        return "Empty";
    }

    std::optional<std::vector<byte>> PhotoHnS::extract(const std::string& path)
    {
        std::optional<std::vector<byte>> data;
        return data;
    }


} // Yps