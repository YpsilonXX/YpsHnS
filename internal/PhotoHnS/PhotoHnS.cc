// PhotoHnS.cc
#include "PhotoHnS.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <filesystem>  // For filename()
#include <cstdio>      // For FILE*
#include <stdexcept>   // For runtime_error
#include <cstring>     // For std::memcpy, std::strncpy

namespace Yps
{
    bool PhotoHnS::has_usable_alpha(const byte *image, int32_t width, int32_t height, int32_t channels)
    {
        if (channels != 4)
            return false;
        // Check alpha channel (every 4th byte) for full opacity (255).
        // Use uint32_t for i to avoid overflow in large images.
        for (uint32_t i = 3; i < static_cast<uint32_t>(width) * height * channels; i += channels)
            if (image[i] != 255)
                return false;
        return true;
    }

    std::optional<std::string> PhotoHnS::embed(const std::vector<byte>& data, const std::string& path, const std::string& out_path)
    {
        // Local variables instead of EmbedData struct for simplicity and direct control.
        MetaData meta{};
        std::array<byte, SHA256_DIGEST_LENGTH> key = AuthorKey::getInstance().get_key();
        std::vector<byte> encrypted_data;

        // Fill metadata (only filename, not full path — safer).
        meta.container = ContainerType::PHOTO;
        // Use strncpy to safely copy into fixed-size char array; truncate if too long.
        std::string filename_str = std::filesystem::path(path).filename().string();
        if (filename_str.size() >= 256) {
            std::cerr << CLI_RED << "PhotoHnS::embed(): Filename too long (max 255 chars): " << filename_str << CLI_RESET << std::endl;
            return std::nullopt;
        }
        std::strncpy(meta.filename, filename_str.c_str(), 255);
        meta.filename[255] = '\0';  // Ensure null-termination.
        meta.lsb_mode = LsbMode::NoUsed;  // Will be set in specific embed methods.
        meta.meta_size = sizeof(MetaData);  // Set the size manually to avoid const issues.

        // Encryption (key from AuthorKey).
        AES256Encryption::getInstance().set_key(key);
        encrypted_data = AES256Encryption::getInstance().encrypt(data);

        // write_size: encrypted + sizeof(MetaData).
        meta.write_size = encrypted_data.size() + sizeof(MetaData);

        // Path validation.
        auto ext_opt = validate_path(path);
        if (!ext_opt) {
            std::cerr << CLI_RED << "PhotoHnS::embed(): Invalid path: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        std::string filetype = ext_opt.value();

        // Support PNG and JPEG; delegate to specific embed methods.
        if (filetype == "png") {
            meta.ext = Extension::PNG;
            return png_embed(path, out_path, encrypted_data, meta);
        } else if (filetype == "jpg" || filetype == "jpeg") {
            meta.ext = Extension::JPEG;
            meta.lsb_mode = LsbMode::OneBit;  // Only 1-bit mode for DCT.
            return jpg_embed(path, out_path, encrypted_data, meta);
        }

        std::cerr << CLI_RED << "PhotoHnS::embed(): Unsupported extension: " << filetype << CLI_RESET << std::endl;
        return std::nullopt;
    }

    std::optional<std::string> PhotoHnS::png_embed(const std::string& path, const std::string& out_path,
                                                   const std::vector<byte>& encrypted_data, MetaData& meta)
    {
        // Load image (RAII: free at end).
        int32_t width, height, channels;
        byte* image = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!image) {
            std::cerr << CLI_RED << "Error: Failed to load PNG: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        auto free_image = [](byte* p) noexcept { stbi_image_free(p); };
        std::unique_ptr<byte, decltype(free_image)> image_guard(image, free_image);

        // Optional: Check alpha for usability (full opacity reduces artifacts).
        if (channels == 4 && !has_usable_alpha(image, width, height, channels)) {
            std::cout << CLI_YELLOW << "Warning: PNG has variable alpha — artifacts may appear in transparent areas." << CLI_RESET << std::endl;
        }

        // Capacity calculation (image bytes = bits for 1-bit LSB).
        uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;
        uint64_t data_bytes = encrypted_data.size() + sizeof(MetaData);
        uint64_t total_bits = data_bytes * 8ULL;

        // Mode selection (strict <= for safety).
        if (total_bits <= img_bytes) {
            meta.lsb_mode = LsbMode::OneBit;
        } else if (total_bits <= img_bytes * 2ULL) {
            std::cout << CLI_YELLOW << "Warning: Using LsbMode::TwoBits — artifacts may be visible." << CLI_RESET << std::endl;
            meta.lsb_mode = LsbMode::TwoBits;
        } else {
            std::cerr << CLI_RED << "Error: Insufficient capacity in PNG (needed " << total_bits
                      << " bits, available ~" << img_bytes * 2 << ")." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Prepare full data to embed: meta + encrypted.
        std::vector<byte> full_data(sizeof(MetaData) + encrypted_data.size());
        std::memcpy(full_data.data(), &meta, sizeof(MetaData));
        std::memcpy(full_data.data() + sizeof(MetaData), encrypted_data.data(), encrypted_data.size());

        // Embed using selected mode.
        if (meta.lsb_mode == LsbMode::OneBit) {
            lsb_one_bit(image, full_data, img_bytes);
        } else if (meta.lsb_mode == LsbMode::TwoBits) {
            lsb_two_bit(image, full_data, img_bytes);
        }

        // Write output PNG.
        if (!stbi_write_png(out_path.c_str(), width, height, channels, image, width * channels)) {
            std::cerr << CLI_RED << "Error: Failed to write PNG: " << out_path << CLI_RESET << std::endl;
            return std::nullopt;
        }

        return out_path;
    }

    std::optional<std::string> PhotoHnS::jpg_embed(const std::string& path, const std::string& out_path,
                                                   const std::vector<byte>& encrypted_data, const MetaData& meta)
    {
        // Prepare full data to embed: meta + encrypted.
        std::vector<byte> full_data(sizeof(MetaData) + encrypted_data.size());
        std::memcpy(full_data.data(), &meta, sizeof(MetaData));
        std::memcpy(full_data.data() + sizeof(MetaData), encrypted_data.data(), encrypted_data.size());

        // JPEG compression setup (RAII).
        JpegDecompressRAII decompress;
        FILE* infile = std::fopen(path.c_str(), "rb");
        if (!infile) {
            std::cerr << CLI_RED << "Error: Failed to open JPEG: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        auto close_infile = [](FILE* f) { std::fclose(f); };
        std::unique_ptr<FILE, decltype(close_infile)> infile_guard(infile, close_infile);

        jpeg_stdio_src(&decompress.cinfo, infile);
        if (jpeg_read_header(&decompress.cinfo, TRUE) == JPEG_SUSPENDED) {
            std::cerr << CLI_RED << "Error: JPEG header suspended." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        jvirt_barray_ptr* coef_arrays = jpeg_read_coefficients(&decompress.cinfo);
        if (!coef_arrays) {
            std::cerr << CLI_RED << "Error: Failed to read JPEG coefficients." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Embed into DCT coefficients.
        dct_lsb_embed(coef_arrays, decompress.cinfo, full_data);

        // Write modified JPEG.
        JpegCompressRAII compress;
        FILE* outfile = std::fopen(out_path.c_str(), "wb");
        if (!outfile) {
            std::cerr << CLI_RED << "Error: Failed to open output JPEG: " << out_path << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }
        auto close_outfile = [](FILE* f) { std::fclose(f); };
        std::unique_ptr<FILE, decltype(close_outfile)> outfile_guard(outfile, close_outfile);

        jpeg_stdio_dest(&compress.cinfo, outfile);
        jpeg_copy_critical_parameters(&decompress.cinfo, &compress.cinfo);
        jpeg_write_coefficients(&compress.cinfo, coef_arrays);

        jpeg_finish_compress(&compress.cinfo);
        jpeg_finish_decompress(&decompress.cinfo);

        return out_path;
    }

    std::optional<std::vector<byte>> PhotoHnS::extract(const std::string& path)
    {
        // Local variables for extraction.
        MetaData meta{};
        std::array<byte, SHA256_DIGEST_LENGTH> key = AuthorKey::getInstance().get_key();
        std::vector<byte> encrypted_data;
        std::vector<byte> plain_data;

        // Path validation.
        auto ext_opt = validate_path(path);
        if (!ext_opt) {
            std::cerr << CLI_RED << "PhotoHnS::extract(): Invalid path: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        std::string filetype = ext_opt.value();

        // Try pixel-based extraction first (for PNG or JPEG with pixel meta).
        // Load image for pixel access.
        int32_t width, height, channels;
        byte* image = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!image) {
            std::cerr << CLI_RED << "Error: Failed to load image: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        auto free_image = [](byte* p) noexcept { stbi_image_free(p); };
        std::unique_ptr<byte, decltype(free_image)> image_guard(image, free_image);

        uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;

        // Attempt to extract meta from pixels.
        auto meta_opt = extract_meta_from_pixels(image, img_bytes);
        if (meta_opt) {
            meta = *meta_opt;
            // Validate meta.
            if (meta.container != ContainerType::PHOTO || meta.write_size < sizeof(MetaData) || meta.meta_size != sizeof(MetaData)) {
                std::cerr << CLI_RED << "Error: Invalid metadata from pixels." << CLI_RESET << std::endl;
            } else {
                // Extract full data from pixels using detected mode.
                auto full_data_opt = extract_data_from_pixels(image, img_bytes, meta.write_size, meta.lsb_mode);
                if (full_data_opt && full_data_opt->size() == meta.write_size) {
                    // Skip meta in full_data to get encrypted.
                    encrypted_data.assign(full_data_opt->begin() + sizeof(MetaData), full_data_opt->end());
                    // Decrypt.
                    AES256Encryption::getInstance().set_key(key);
                    plain_data = AES256Encryption::getInstance().decrypt(encrypted_data);
                    std::cout << CLI_GREEN << "Extracted " << plain_data.size() << " bytes from pixels." << CLI_RESET << std::endl;
                    return plain_data;
                }
            }
        }

        // Fallback to DCT for JPEG.
        if (filetype == "jpg" || filetype == "jpeg") {
            image_guard.reset();  // Free pixel image as not needed.
            return jpg_extract(path, key);
        }

        std::cerr << CLI_RED << "Error: No valid data extracted." << CLI_RESET << std::endl;
        return std::nullopt;
    }

    std::optional<std::vector<byte>> PhotoHnS::jpg_extract(const std::string& path,
                                                           const std::array<byte, SHA256_DIGEST_LENGTH>& key)
    {
        JpegDecompressRAII decompress;
        FILE* infile = std::fopen(path.c_str(), "rb");
        if (!infile) {
            std::cerr << CLI_RED << "Error: Failed to open JPEG: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        auto close_infile = [](FILE* f) { std::fclose(f); };
        std::unique_ptr<FILE, decltype(close_infile)> infile_guard(infile, close_infile);

        jpeg_stdio_src(&decompress.cinfo, infile);
        if (jpeg_read_header(&decompress.cinfo, TRUE) == JPEG_SUSPENDED) {
            std::cerr << CLI_RED << "Error: JPEG header suspended." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        jvirt_barray_ptr* coef_arrays = jpeg_read_coefficients(&decompress.cinfo);
        if (!coef_arrays) {
            std::cerr << CLI_RED << "Error: Failed to read JPEG coefficients." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Extract meta from DCT (1-bit LSB).
        auto meta_dct_opt = dct_lsb_extract(coef_arrays, decompress.cinfo, sizeof(MetaData));
        if (!meta_dct_opt || meta_dct_opt->size() != sizeof(MetaData)) {
            std::cerr << CLI_RED << "Error: Failed to extract JPEG metadata from DCT." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }
        MetaData meta;
        std::memcpy(&meta, meta_dct_opt->data(), sizeof(MetaData));

        // Validate DCT meta.
        if (meta.container != ContainerType::PHOTO ||
            meta.ext != Extension::JPEG ||
            meta.write_size < sizeof(MetaData) ||
            meta.meta_size != sizeof(MetaData)) {
            std::cerr << CLI_RED << "Error: Invalid JPEG metadata from DCT." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }

        // Full extraction from DCT.
        auto full_dct_opt = dct_lsb_extract(coef_arrays, decompress.cinfo, meta.write_size);
        if (!full_dct_opt || full_dct_opt->size() != meta.write_size) {
            std::cerr << CLI_RED << "Error: Incomplete full extraction from JPEG DCT." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }

        // Encrypted data (skip meta).
        std::vector<byte> encrypted_data(full_dct_opt->begin() + sizeof(MetaData), full_dct_opt->end());

        jpeg_finish_decompress(&decompress.cinfo);

        // Decrypt.
        AES256Encryption::getInstance().set_key(key);
        std::vector<byte> plain_data = AES256Encryption::getInstance().decrypt(encrypted_data);

        std::cout << CLI_GREEN << "Extracted " << plain_data.size() << " bytes from JPEG DCT." << CLI_RESET << std::endl;
        return plain_data;
    }

    std::optional<MetaData> PhotoHnS::extract_meta_from_pixels(const byte* image, uint64_t img_bytes)
    {
        // Try 1-bit mode first for meta.
        auto data_opt = lsb_extract_one_bit(image, sizeof(MetaData), img_bytes);
        if (data_opt) {
            MetaData meta;
            std::memcpy(&meta, data_opt->data(), sizeof(MetaData));
            if (meta.lsb_mode == LsbMode::OneBit && meta.meta_size == sizeof(MetaData)) {
                return meta;
            }
        }

        // Try 2-bit mode for meta.
        data_opt = lsb_extract_two_bit(image, sizeof(MetaData), img_bytes);
        if (data_opt) {
            MetaData meta;
            std::memcpy(&meta, data_opt->data(), sizeof(MetaData));
            if (meta.lsb_mode == LsbMode::TwoBits && meta.meta_size == sizeof(MetaData)) {
                return meta;
            }
        }

        return std::nullopt;
    }

    std::optional<std::vector<byte>> PhotoHnS::extract_data_from_pixels(const byte* image, uint64_t img_bytes,
                                                                        uint64_t data_bytes, LsbMode mode)
    {
        if (mode == LsbMode::OneBit) {
            return lsb_extract_one_bit(image, data_bytes, img_bytes);
        } else if (mode == LsbMode::TwoBits) {
            return lsb_extract_two_bit(image, data_bytes, img_bytes);
        }
        return std::nullopt;
    }

    void PhotoHnS::lsb_one_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes)
    {
        uint64_t bit_index = 0;
        uint64_t data_bits = data.size() * 8ULL;
        for (uint64_t i = 0; i < img_bytes && bit_index < data_bits; ++i) {
            // Clear LSB and set from data bit (MSB-first).
            byte bit = (data[bit_index / 8] >> (7 - (bit_index % 8))) & 1;
            image[i] = (image[i] & 0xFE) | bit;
            ++bit_index;
        }
    }

    void PhotoHnS::lsb_two_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes)
    {
        uint64_t bit_index = 0;
        uint64_t data_bits = data.size() * 8ULL;
        for (uint64_t i = 0; i < img_bytes && bit_index < data_bits; ++i) {
            // Clear 2 LSBs and set from data bits (MSB-first).
            byte bits = (data[bit_index / 8] >> (6 - (bit_index % 8))) & 3;  // Get 2 bits.
            image[i] = (image[i] & 0xFC) | bits;
            bit_index += 2;
        }
    }

    std::optional<std::vector<byte>> PhotoHnS::lsb_extract_one_bit(const byte* image, uint64_t data_bytes, uint64_t img_bytes) const
    {
        std::vector<byte> data(data_bytes, 0);
        uint64_t bit_index = 0;
        uint64_t data_bits = data_bytes * 8ULL;
        if (data_bits > img_bytes) return std::nullopt;  // Insufficient capacity.

        for (uint64_t i = 0; i < img_bytes && bit_index < data_bits; ++i) {
            byte bit = image[i] & 1;
            data[bit_index / 8] |= (bit << (7 - (bit_index % 8)));
            ++bit_index;
        }
        return (bit_index == data_bits) ? std::make_optional(data) : std::nullopt;
    }

    std::optional<std::vector<byte>> PhotoHnS::lsb_extract_two_bit(const byte* image, uint64_t data_bytes, uint64_t img_bytes) const
    {
        std::vector<byte> data(data_bytes, 0);
        uint64_t bit_index = 0;
        uint64_t data_bits = data_bytes * 8ULL;
        if (data_bits > img_bytes * 2ULL) return std::nullopt;  // Insufficient capacity.

        for (uint64_t i = 0; i < img_bytes && bit_index < data_bits; ++i) {
            byte bits = image[i] & 3;
            data[bit_index / 8] |= (bits << (6 - (bit_index % 8)));  // Set 2 bits MSB-first.
            bit_index += 2;
        }
        return (bit_index == data_bits) ? std::make_optional(data) : std::nullopt;
    }

    void PhotoHnS::dct_lsb_embed(jvirt_barray_ptr* coef_arrays, const jpeg_decompress_struct& cinfo,
                                 const std::vector<byte>& data)
    {
        uint64_t bit_index = 0;
        uint64_t data_bits = data.size() * 8ULL;

        // Iterate over components (Y, Cb, Cr).
        for (int comp = 0; comp < cinfo.num_components; ++comp) {
            jpeg_component_info* comp_info = &cinfo.comp_info[comp];
            uint32_t num_blocks_h = (cinfo.image_width + comp_info->h_samp_factor * DCTSIZE - 1) / (comp_info->h_samp_factor * DCTSIZE);
            uint32_t num_blocks_v = (cinfo.image_height + comp_info->v_samp_factor * DCTSIZE - 1) / (comp_info->v_samp_factor * DCTSIZE);

            for (uint32_t block_y = 0; block_y < num_blocks_v; ++block_y) {
                JBLOCKARRAY block_row = (cinfo.mem->access_virt_barray)((j_common_ptr)&cinfo, coef_arrays[comp], block_y, 1, TRUE);
                for (uint32_t block_x = 0; block_x < num_blocks_h; ++block_x) {
                    JCOEFPTR coeffs = block_row[block_x][0];
                    // Skip DC (coeffs[0]), embed in low-freq AC (zigzag order, first few).
                    for (int k = 1; k < 8 && bit_index < data_bits; ++k) {  // Limit to low-freq for robustness.
                        if (coeffs[k] != 0) {  // Only non-zero coeffs to avoid creating new runs.
                            byte bit = (data[bit_index / 8] >> (7 - (bit_index % 8))) & 1;
                            coeffs[k] = (coeffs[k] & ~1) | bit;  // Set LSB.
                            ++bit_index;
                        }
                    }
                }
            }
            if (bit_index >= data_bits) return;
        }
    }

    std::optional<std::vector<byte>> PhotoHnS::dct_lsb_extract(jvirt_barray_ptr* coef_arrays,
                                                               const jpeg_decompress_struct& cinfo,
                                                               uint64_t data_bytes) const
    {
        std::vector<byte> data(data_bytes, 0);
        uint64_t bit_index = 0;
        uint64_t data_bits = data_bytes * 8ULL;

        // Iterate over components similarly.
        for (int comp = 0; comp < cinfo.num_components; ++comp) {
            jpeg_component_info* comp_info = &cinfo.comp_info[comp];
            uint32_t num_blocks_h = (cinfo.image_width + comp_info->h_samp_factor * DCTSIZE - 1) / (comp_info->h_samp_factor * DCTSIZE);
            uint32_t num_blocks_v = (cinfo.image_height + comp_info->v_samp_factor * DCTSIZE - 1) / (comp_info->v_samp_factor * DCTSIZE);

            for (uint32_t block_y = 0; block_y < num_blocks_v; ++block_y) {
                JBLOCKARRAY block_row = (cinfo.mem->access_virt_barray)((j_common_ptr)&cinfo, coef_arrays[comp], block_y, 1, FALSE);
                for (uint32_t block_x = 0; block_x < num_blocks_h; ++block_x) {
                    JCOEFPTR coeffs = block_row[block_x][0];
                    for (int k = 1; k < 8 && bit_index < data_bits; ++k) {
                        if (coeffs[k] != 0) {
                            byte bit = coeffs[k] & 1;
                            data[bit_index / 8] |= (bit << (7 - (bit_index % 8)));
                            ++bit_index;
                        }
                    }
                }
            }
            if (bit_index >= data_bits) return data;
        }
        return (bit_index == data_bits) ? std::make_optional(data) : std::nullopt;
    }

} // Yps