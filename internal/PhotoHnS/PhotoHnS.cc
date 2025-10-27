#include "PhotoHnS.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <filesystem>  // Для filename()

namespace Yps
{
    bool PhotoHnS::has_usable_alpha(const byte *image, int32_t width, int32_t height, int32_t channels)
    {
        if (channels != 4)
            return false;
        // Проверяем alpha-канал (каждый 4-й байт) на full opacity (255).
        // Используем uint32_t для i, чтобы избежать overflow в больших изображениях.
        for (uint32_t i = 3; i < static_cast<uint32_t>(width) * height * channels; i += channels)
            if (image[i] != 255)
                return false;
        return true;
    }

    std::optional<std::string> PhotoHnS::embed(const std::vector<byte>& data, const std::string& path, const std::string& out_path)
    {
        // Инициализация EmbedData (reset если нужно).
        if (!this->embed_data)
            this->embed_data = std::make_unique<EmbedData>();
        else
            this->embed_data = std::make_unique<EmbedData>();  // Автоматический reset via move.

        // Заполнение метаданных (только имя файла, не полный путь — безопаснее).
        this->embed_data->plain_data = data;
        this->embed_data->meta.container = ContainerType::PHOTO;
        this->embed_data->meta.filename = std::filesystem::path(path).filename().string();
        this->embed_data->key = AuthorKey::getInstance().get_key();

        // Шифрование (ключ из AuthorKey).
        AES256Encryption::getInstance().set_key(this->embed_data->key);
        this->embed_data->encrypt_data = AES256Encryption::getInstance().encrypt(this->embed_data->plain_data);

        // write_size: encrypt + sizeof(MetaData) (временно; для raw-копии).
        this->embed_data->meta.write_size = this->embed_data->encrypt_data.size() + sizeof(MetaData);

        // Валидация пути.
        auto ext_opt = validate_path(path);
        if (!ext_opt) {
            std::cerr << CLI_RED << "PhotoHnS::embed(): Invalid path: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        std::string filetype = ext_opt.value();

        // Поддержка только PNG пока.
        if (filetype == "png") {
            this->embed_data->meta.ext = Extension::PNG;
            return this->png_in(out_path);
        }

        std::cerr << CLI_RED << "PhotoHnS::embed(): Unsupported extension: " << filetype << CLI_RESET << std::endl;
        return std::nullopt;
    }

    std::optional<std::string> PhotoHnS::png_in(const std::string &out_path)
    {
        // Загрузка изображения (RAII: free в конце).
        int32_t width, height, channels;
        byte* image = stbi_load(this->embed_data->meta.filename.c_str(), &width, &height, &channels, 0);
        if (!image) {
            std::cerr << CLI_RED << "Error: Failed to load PNG: " << this->embed_data->meta.filename << CLI_RESET << std::endl;
            return std::nullopt;
        }

        auto free_image = [](byte* p) noexcept { stbi_image_free(p); };
        std::unique_ptr<byte, decltype(free_image)> image_guard(image, free_image);


        // Расчёт ёмкости (байты изображения = биты для 1-bit LSB).
        uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;
        uint64_t data_bytes = this->embed_data->encrypt_data.size() + sizeof(MetaData);
        uint64_t total_bits = data_bytes * 8ULL;

        // Выбор режима (строго <= для безопасности).
        LsbMode mode = LsbMode::NoUsed;
        if (total_bits <= img_bytes) {
            mode = LsbMode::OneBit;
        } else if (total_bits <= img_bytes * 2ULL) {  // Упрощённо; минус meta*8 опционально.
            std::cout << CLI_YELLOW << "Warning: Using LsbMode::TwoBits — artifacts may be visible." << CLI_RESET << std::endl;
            mode = LsbMode::TwoBits;
        } else {
            std::cerr << CLI_RED << "Error: Insufficient capacity in PNG (needed " << total_bits
                      << " bits, available ~" << img_bytes * 2 << ")." << CLI_RESET << std::endl;
            return std::nullopt;
        }
        this->embed_data->meta.lsb_mode = mode;

        // Подготовка full_data
        std::vector<byte> full_data(data_bytes);
        MetaData* m = &this->embed_data->meta;
        std::copy(reinterpret_cast<const byte*>(m), reinterpret_cast<const byte*>(m) + sizeof(MetaData), full_data.begin());
        std::copy(this->embed_data->encrypt_data.begin(), this->embed_data->encrypt_data.end(),
                  full_data.begin() + sizeof(MetaData));

        // Встраивание (с bound-check).
        switch (mode) {
            case LsbMode::OneBit:
                this->lsb_one_bit(image, full_data, img_bytes);
                break;
            case LsbMode::TwoBits:
                this->lsb_two_bit(image, full_data, img_bytes);
                break;
            default:
                return std::nullopt;
        }

        // Сохранение (stride=0 auto).
        int success = stbi_write_png(out_path.c_str(), width, height, channels, image, 0);
        if (!success) {
            std::cerr << CLI_RED << "Error: Failed to write PNG: " << out_path << CLI_RESET << std::endl;
            return std::nullopt;
        }

        std::cout << CLI_GREEN << "Embedded " << data_bytes << " bytes into " << out_path << " (mode: "
                  << static_cast<int>(mode) << ")." << CLI_RESET << std::endl;
        return out_path;
    }

    void PhotoHnS::lsb_one_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes)
    {
        // 1 бит на байт изображения, MSB-first.
        uint64_t total_bits = data.size() * 8ULL;
        if (total_bits > img_bytes) {
            throw std::runtime_error("Internal: Capacity mismatch in lsb_one_bit");  // Не должно быть.
        }

        uint64_t bit_idx = 0;
        for (uint64_t i = 0; i < total_bits; ++i) {
            if (i >= img_bytes) break;  // Extra safety.

            byte current_byte = data[bit_idx / 8];
            int bit_pos_in_byte = bit_idx % 8;
            byte bit = (current_byte >> (7 - bit_pos_in_byte)) & 1;
            image[i] = (image[i] & 0xFE) | bit;
            ++bit_idx;
        }
    }

    void PhotoHnS::lsb_two_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes)
    {
        uint64_t total_bits = data.size() * 8ULL;
        uint64_t meta_bits = sizeof(MetaData) * 8ULL;  // Биты для meta (raw sizeof).
        uint64_t bit_idx = 0;
        uint64_t img_idx = 0;

        // Первая часть: meta в 1-bit режиме (безопасно, без overflow).
        for (; bit_idx < meta_bits && img_idx < img_bytes; ++img_idx) {
            byte current_byte = data[bit_idx / 8];
            int bit_pos_in_byte = bit_idx % 8;
            byte bit = (current_byte >> (7 - bit_pos_in_byte)) & 1;
            image[img_idx] = (image[img_idx] & 0xFE) | bit;
            ++bit_idx;
        }

        // Вторая часть: остаток в 2-bit (пары битов, MSB-first).
        for (; bit_idx < total_bits && img_idx < img_bytes; ++img_idx) {
            if (bit_idx + 1 >= total_bits) break;  // Нечётные биты — стоп.

            byte current_byte = data[bit_idx / 8];
            int bit_pos_in_byte = bit_idx % 8;
            byte bits = (current_byte >> (6 - bit_pos_in_byte)) & 0x03;  // 2 бита.
            image[img_idx] = (image[img_idx] & 0xFC) | bits;
            bit_idx += 2;
        }

        if (bit_idx < total_bits) {
            std::cerr << CLI_YELLOW << "Warning: Incomplete embed in TwoBits (used " << bit_idx << "/" << total_bits << " bits)." << CLI_RESET << std::endl;
        }
    }

    std::optional<std::string> PhotoHnS::png_out(byte* image, MetaData& meta, const std::string& path)
    {
        // Подготовка full_data
        uint64_t data_bytes = meta.write_size;
        std::vector<byte> full_data(data_bytes, 0);
        std::copy(reinterpret_cast<const byte*>(&meta), reinterpret_cast<const byte*>(&meta) + sizeof(MetaData),
                  full_data.begin());

        uint64_t total_bits = data_bytes * 8ULL;
        uint64_t meta_bits = sizeof(MetaData) * 8ULL;
        uint64_t encrypt_bits_start = meta_bits;
        uint64_t encrypt_bytes = data_bytes - sizeof(MetaData);
        uint64_t encrypt_bits = encrypt_bytes * 8ULL;

        // Старт после meta (байты изображения, использованные для meta).
        uint64_t img_idx = meta_bits;  // Ключевой фикс: *8, не sizeof.
        uint64_t bit_pos = encrypt_bits_start;
        bool success = true;

        if (meta.lsb_mode == LsbMode::OneBit) {
            // 1 бит на байт, MSB-first.
            for (; bit_pos < total_bits && img_idx < total_bits; ++img_idx, ++bit_pos) {  // total_bits как upper bound (overapprox).
                if (img_idx >= total_bits) { success = false; break; }  // Safety.

                byte bit = image[img_idx] & 0x01;
                uint64_t byte_idx = bit_pos / 8ULL;  // Фикс: без + meta_size!
                if (byte_idx >= full_data.size()) { success = false; break; }

                int bit_offset = bit_pos % 8;
                full_data[byte_idx] |= (bit << (7 - bit_offset));
            }
        } else if (meta.lsb_mode == LsbMode::TwoBits) {
            // 2 бита на байт (после meta в 1-bit).
            for (; bit_pos + 1 < total_bits && img_idx < total_bits; img_idx++, bit_pos += 2) {
                if (img_idx >= total_bits) { success = false; break; }

                byte bits = image[img_idx] & 0x03;
                uint64_t byte_idx = bit_pos / 8ULL;  // Фикс: без +!
                if (byte_idx >= full_data.size()) { success = false; break; }

                int bit_offset = bit_pos % 8;
                int shift = 6 - bit_offset;  // Match embed: 6,4,2,0.
                full_data[byte_idx] |= (static_cast<byte>(bits) << shift);
            }
        } else {
            std::cerr << CLI_RED << "Error: Unsupported LsbMode: " << static_cast<int>(meta.lsb_mode) << CLI_RESET << std::endl;
            return std::nullopt;
        }

        if (!success || bit_pos < total_bits) {
            std::cerr << CLI_RED << "Error: Incomplete extraction (mode: " << static_cast<int>(meta.lsb_mode)
                      << ", extracted " << bit_pos << "/" << total_bits << " bits)." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Извлечение encrypt_data (с meta уже в full_data).
        std::vector<byte> encrypt_data(full_data.begin() + sizeof(MetaData), full_data.end());
        this->embed_data->encrypt_data = std::move(encrypt_data);

        // Декрипт.
        AES256Encryption::getInstance().set_key(this->embed_data->key);
        this->embed_data->plain_data = AES256Encryption::getInstance().decrypt(this->embed_data->encrypt_data);

        std::cout << CLI_GREEN << "Extracted " << this->embed_data->plain_data.size() << " bytes (mode: "
                  << static_cast<int>(meta.lsb_mode) << ")." << CLI_RESET << std::endl;
        return path;
    }

    std::optional<std::vector<byte>> PhotoHnS::extract(const std::string& path)
    {
        // Инициализация embed_data (если нет — fail).
        if (!this->embed_data) {
            std::cerr << CLI_RED << "Error: No EmbedData context in extract." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Загрузка (RAII).
        int32_t width, height, channels;
        byte* image = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!image) {
            std::cerr << CLI_RED << "Error: Failed to load PNG: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        auto free_image = [](byte* p) noexcept { stbi_image_free(p); };
        std::unique_ptr<byte, decltype(free_image)> image_guard(image, free_image);

        uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;
        if (img_bytes < sizeof(MetaData) * 8ULL) {
            std::cerr << CLI_RED << "Error: Image too small for metadata: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Извлечение meta (1-bit, MSB-first). Фикс: vector размером sizeof (байты), не биты!
        size_t meta_byte_size = sizeof(MetaData);
        std::vector<byte> meta_bytes(meta_byte_size, 0);  // 48 байт.
        size_t bit_pos = 0;
        for (size_t i = 0; i < meta_byte_size * 8ULL; ++i) {
            if (i >= img_bytes) break;  // Safety.

            byte bit = image[i] & 0x01;
            size_t byte_idx = bit_pos / 8;
            size_t bit_offset = bit_pos % 8;
            meta_bytes[byte_idx] |= (bit << (7 - bit_offset));
            ++bit_pos;
        }

        // Копируем в meta
        std::copy(meta_bytes.begin(), meta_bytes.end(), reinterpret_cast<byte*>(&this->embed_data->meta));

        // Валидация meta.
        if (this->embed_data->meta.container != ContainerType::PHOTO) {
            std::cerr << CLI_RED << "Error: Invalid metadata (not PHOTO)." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Извлечение по типу.
        std::optional<std::string> result;
        switch (this->embed_data->meta.ext) {
            case Extension::PNG:
                result = png_out(image, this->embed_data->meta, path);
                break;
            case Extension::JPEG:
                std::cerr << CLI_YELLOW << "Warning: JPEG not supported yet." << CLI_RESET << std::endl;
                return std::nullopt;
            default:
                return std::nullopt;
        }

        if (!result.has_value()) {
            return std::nullopt;
        }

        return this->embed_data->plain_data;  // Теперь safe: set только при успехе.
    }

} // Yps