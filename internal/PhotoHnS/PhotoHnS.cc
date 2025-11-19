#include "PhotoHnS.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <filesystem>  // For filename()
#include <cstdio>      // For FILE*
#include <stdexcept>   // For runtime_error
#include <cstring>     // For std::memcpy

namespace Yps
{
    struct StbImageDeleter {
        void operator()(unsigned char* data) const noexcept {
            if (data) stbi_image_free(data);
        }
    };
    using StbImageUniquePtr = std::unique_ptr<unsigned char[], StbImageDeleter>;

    bool PhotoHnS::has_usable_alpha(const byte *image, int32_t width, int32_t height, int32_t channels)
    {
        if (channels != 4) return false;
        for (uint32_t i = 3; i < static_cast<uint32_t>(width) * height * channels; i += channels)
            if (image[i] != 255) return false;
        return true;
    }

    /* -------------------------------------------------------------------------- */
    /*  Public embed entry point – old signature (no payload_filename argument)   */
    /* -------------------------------------------------------------------------- */
    std::optional<std::string> PhotoHnS::embed(const std::vector<byte>& data,
                                               const std::string& container_path,
                                               const std::string& out_path)
    {
        embed_data = std::make_unique<EmbedData>();

        // If the filename field is empty (set by MainWindow), fall back to container name
        if (embed_data->meta.filename[0] == '\0') {
            std::string fallback = std::filesystem::path(container_path).filename().string();
            std::strncpy(embed_data->meta.filename, fallback.c_str(),
                         sizeof(embed_data->meta.filename) - 1);
            embed_data->meta.filename[sizeof(embed_data->meta.filename) - 1] = '\0';
        }

        embed_data->plain_data = data;
        embed_data->meta.container = ContainerType::PHOTO;

        embed_data->key = AuthorKey::getInstance().get_key();
        AES256Encryption::getInstance().set_key(embed_data->key);
        embed_data->encrypt_data = AES256Encryption::getInstance().encrypt(data);

        const auto ext_opt = validate_path(container_path);
        if (!ext_opt) {
            std::cerr << "[PhotoHnS] Invalid container path\n";
            return std::nullopt;
        }
        const std::string ext = ext_opt.value();

        if (ext == "png") {
            embed_data->meta.ext = Extension::PNG;
            embed_data->meta.lsb_mode = LsbMode::OneBit;          // Always 1-bit for reliability
            return png_in(out_path);
        }
        if (ext == "jpg" || ext == "jpeg") {
            embed_data->meta.ext = Extension::JPEG;
            embed_data->meta.lsb_mode = LsbMode::OneBit;
            return jpg_in(out_path);
        }

        std::cerr << "[PhotoHnS] Unsupported container extension: " << ext << '\n';
        return std::nullopt;
    }

    /* -------------------------------------------------------------------------- */
    /*  PNG embedding – only 1-bit LSB (simpler and more robust)                  */
    /* -------------------------------------------------------------------------- */
    std::optional<std::string> PhotoHnS::png_in(const std::string& out_path)
    {
        int width, height, channels;
        StbImageUniquePtr img(stbi_load(embed_data->meta.filename,
                                        &width, &height, &channels, 0));
        if (!img) {
            std::cerr << "[PhotoHnS] Failed to load PNG: " << embed_data->meta.filename << '\n';
            return std::nullopt;
        }

        const uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;

        // Build payload: metadata + encrypted data
        std::vector<byte> payload;
        payload.resize(sizeof(MetaData));
        std::memcpy(payload.data(), &embed_data->meta, sizeof(MetaData));
        payload.insert(payload.end(),
                       embed_data->encrypt_data.begin(),
                       embed_data->encrypt_data.end());

        const uint64_t required_bits = payload.size() * 8;
        if (required_bits > img_bytes) {
            std::cerr << "[PhotoHnS] Not enough capacity in PNG (need "
                      << required_bits << " bits, have " << img_bytes << ")\n";
            return std::nullopt;
        }

        lsb_one_bit(img.get(), payload, img_bytes);
        embed_data->meta.lsb_mode = LsbMode::OneBit;

        const int stride = width * channels;
        if (!stbi_write_png(out_path.c_str(), width, height, channels,
                            img.get(), stride)) {
            std::cerr << "[PhotoHnS] stbi_write_png failed\n";
            return std::nullopt;
                            }

        return out_path;
    }

    /* -------------------------------------------------------------------------- */
    /*  JPEG embedding – DCT LSB (unchanged core logic, only minor safety fixes)   */
    /* -------------------------------------------------------------------------- */
    std::optional<std::string> PhotoHnS::jpg_in(const std::string& out_path)
    {
        std::vector<byte> full_data;
        full_data.resize(sizeof(MetaData));
        std::memcpy(full_data.data(), &embed_data->meta, sizeof(MetaData));
        full_data.insert(full_data.end(),
                         embed_data->encrypt_data.begin(),
                         embed_data->encrypt_data.end());

        JpegDecompressRAII decompress;
        FILE* infile = std::fopen(embed_data->meta.filename, "rb");
        if (!infile) {
            std::cerr << "[PhotoHnS] Cannot open JPEG container\n";
            return std::nullopt;
        }
        // Simple RAII fclose
        auto close_file = [](FILE* f) { if (f) std::fclose(f); };
        std::unique_ptr<FILE, decltype(close_file)> fg(infile, close_file);

        jpeg_stdio_src(&decompress.cinfo, infile);
        jpeg_read_header(&decompress.cinfo, TRUE);
        jvirt_barray_ptr* coef_arrays = jpeg_read_coefficients(&decompress.cinfo);
        if (!coef_arrays) {
            std::cerr << "[PhotoHnS] jpeg_read_coefficients failed\n";
            return std::nullopt;
        }

        dct_lsb_embed(coef_arrays, decompress.cinfo, full_data);

        JpegCompressRAII compress;
        FILE* outfile = std::fopen(out_path.c_str(), "wb");
        if (!outfile) return std::nullopt;
        std::unique_ptr<FILE, decltype(close_file)> fg_out(outfile, close_file);

        jpeg_stdio_dest(&compress.cinfo, outfile);
        compress.cinfo.image_width      = decompress.cinfo.image_width;
        compress.cinfo.image_height     = decompress.cinfo.image_height;
        compress.cinfo.input_components = decompress.cinfo.num_components;
        compress.cinfo.in_color_space   = decompress.cinfo.jpeg_color_space;

        jpeg_set_defaults(&compress.cinfo);
        jpeg_set_quality(&compress.cinfo, 95, TRUE);
        jpeg_copy_critical_parameters(&decompress.cinfo, &compress.cinfo);

        jpeg_write_coefficients(&compress.cinfo, coef_arrays);
        jpeg_finish_compress(&compress.cinfo);
        jpeg_destroy_compress(&compress.cinfo);

        return out_path;
    }

    void PhotoHnS::lsb_one_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes)
    {
        // 1 bit per image byte, MSB-first.
        uint64_t total_bits = data.size() * 8ULL;
        if (total_bits > img_bytes) {
            throw std::runtime_error("Internal: Capacity mismatch in lsb_one_bit");  // Should not happen.
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
        uint64_t meta_bits = sizeof(MetaData) * 8ULL;  // Bits for metadata (raw sizeof).
        uint64_t bit_idx = 0;
        uint64_t img_idx = 0;

        // First part: metadata in 1-bit mode (safe, no overflow).
        for (; bit_idx < meta_bits && img_idx < img_bytes; ++img_idx) {
            byte current_byte = data[bit_idx / 8];
            int bit_pos_in_byte = bit_idx % 8;
            byte bit = (current_byte >> (7 - bit_pos_in_byte)) & 1;
            image[img_idx] = (image[img_idx] & 0xFE) | bit;
            ++bit_idx;
        }

        // Second part: remainder in 2-bit mode (pairs of bits, MSB-first).
        for (; bit_idx < total_bits && img_idx < img_bytes; ++img_idx) {
            if (bit_idx + 1 >= total_bits) break;  // Odd bits — stop.

            byte current_byte = data[bit_idx / 8];
            int bit_pos_in_byte = bit_idx % 8;
            byte bits = (current_byte >> (6 - bit_pos_in_byte)) & 0x03;  // 2 bits.
            image[img_idx] = (image[img_idx] & 0xFC) | bits;
            bit_idx += 2;
        }

        if (bit_idx < total_bits) {
            std::cerr << CLI_YELLOW << "Warning: Incomplete embed in TwoBits (used " << bit_idx << "/" << total_bits << " bits)." << CLI_RESET << std::endl;
        }
    }

    void PhotoHnS::dct_lsb_embed(jvirt_barray_ptr* coef_arrays, const jpeg_decompress_struct& cinfo,
                                 const std::vector<byte>& data)
    {
        uint64_t bit_idx = 0;
        uint64_t total_bits = data.size() * 8ULL;

        // Iteration: components → block rows → blocks → AC coeffs (skip DC=0).
        for (int ci = 0; ci < cinfo.num_components && bit_idx < total_bits; ++ci) {
            jpeg_component_info* comp = cinfo.comp_info + ci;

            for (JDIMENSION blk_row = 0; blk_row < comp->height_in_blocks && bit_idx < total_bits; ++blk_row) {
                // Access row (write mode: TRUE — modify in-place).
                JBLOCKARRAY block_array = (JBLOCKARRAY) (*cinfo.mem->access_virt_barray)
                    ((j_common_ptr) &cinfo, coef_arrays[ci], blk_row, 1, TRUE);  // TRUE for write.
                if (block_array == nullptr) {
                    std::cerr << CLI_RED << "Error: Failed to access DCT block row " << blk_row << " for embedding." << CLI_RESET << std::endl;
                    return;  // Abort gracefully.
                }
                JBLOCKROW block_row = block_array[0];

                for (JDIMENSION blk_col = 0; blk_col < comp->width_in_blocks && bit_idx < total_bits; ++blk_col) {
                    JBLOCK* block = block_row + blk_col;

                    for (int k = 1; k < DCTSIZE2 && bit_idx < total_bits; ++k) {  // Skip DC (k=0).
                        // LSB embed: clear bit, set to data_bit (MSB-first).
                        uint64_t byte_idx = bit_idx / 8ULL;
                        int bit_offset = bit_idx % 8;
                        byte data_bit = (data[byte_idx] >> (7 - bit_offset)) & 1;

                        JCOEF& coef = (*block)[k];
                        coef = (coef & ~1) | static_cast<JCOEF>(data_bit);  // Set LSB.

                        // Clamp for DCT-range (avoid overflow in Huffman).
                        if (coef > 1023) coef = 1023;
                        else if (coef < -1024) coef = -1024;

                        ++bit_idx;
                    }
                }
            }
        }

        if (bit_idx < total_bits) {
            std::cerr << CLI_YELLOW << "Warning: Partial embed (" << bit_idx << "/" << total_bits << " bits)." << CLI_RESET << std::endl;
        }
    }

    std::optional<std::string> PhotoHnS::png_out(byte* image, MetaData& meta, const std::string& path)
    {
        // Prepare full_data with metadata prefix.
        uint64_t data_bytes = meta.write_size;
        std::vector<byte> full_data(data_bytes, 0);
        std::copy(reinterpret_cast<const byte*>(&meta), reinterpret_cast<const byte*>(&meta) + sizeof(MetaData),
                  full_data.begin());

        uint64_t total_bits = data_bytes * 8ULL;
        uint64_t meta_bits = sizeof(MetaData) * 8ULL;
        bool success = true;

        // Start extraction after metadata bits (image indices align with bit positions for 1-bit, adjusted for 2-bit).
        // Note: Use total_bits as upper bound (assumes sufficient capacity from embed; real img_bytes larger).
        uint64_t img_idx = meta_bits;
        uint64_t bit_pos = meta_bits;

        if (meta.lsb_mode == LsbMode::OneBit) {
            // 1 bit per image byte, MSB-first.
            for (; bit_pos < total_bits && img_idx < total_bits; ++img_idx, ++bit_pos) {
                if (img_idx >= total_bits) { success = false; break; }  // Safety.

                byte bit = image[img_idx] & 0x01;
                uint64_t byte_idx = bit_pos / 8ULL;
                if (byte_idx >= full_data.size()) { success = false; break; }

                int bit_offset = bit_pos % 8;
                full_data[byte_idx] |= (bit << (7 - bit_offset));
            }
        } else if (meta.lsb_mode == LsbMode::TwoBits) {
            // 2 bits per image byte (after metadata in 1-bit); img_idx advances per pair.
            for (; bit_pos + 1 < total_bits && img_idx < total_bits; ++img_idx, bit_pos += 2) {
                if (img_idx >= total_bits) { success = false; break; }

                byte bits = image[img_idx] & 0x03;
                uint64_t byte_idx = bit_pos / 8ULL;
                if (byte_idx >= full_data.size()) { success = false; break; }

                int bit_offset = bit_pos % 8;
                int shift = 6 - bit_offset;  // Matches embed: shifts for 6,4,2,0 aligning to MSB.
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

        // Extract encrypted data (metadata already in full_data).
        std::vector<byte> encrypt_data(full_data.begin() + sizeof(MetaData), full_data.end());
        this->embed_data->encrypt_data = std::move(encrypt_data);

        // Decrypt.
        AES256Encryption::getInstance().set_key(this->embed_data->key);
        this->embed_data->plain_data = AES256Encryption::getInstance().decrypt(this->embed_data->encrypt_data);

        std::cout << CLI_GREEN << "Extracted " << this->embed_data->plain_data.size() << " bytes (mode: "
                  << static_cast<int>(meta.lsb_mode) << ")." << CLI_RESET << std::endl;
        return path;
    }

    std::optional<std::string> PhotoHnS::jpg_out(const std::string& path)
    {
        // RAII decompress.
        JpegDecompressRAII decompress;
        FILE* infile = std::fopen(path.c_str(), "rb");
        if (!infile) {
            std::cerr << CLI_RED << "Error: Failed to open JPEG: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        auto close_infile = [](FILE* f) { std::fclose(f); };
        std::unique_ptr<FILE, decltype(close_infile)> infile_guard(infile, close_infile);

        // Set up standard input source and read header.
        jpeg_stdio_src(&decompress.cinfo, infile);
        if (jpeg_read_header(&decompress.cinfo, TRUE) == JPEG_SUSPENDED) {
            std::cerr << CLI_RED << "Error: JPEG header read suspended in extract." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Read coefficients (DCT blocks).
        jvirt_barray_ptr* coef_arrays = jpeg_read_coefficients(&decompress.cinfo);
        if (!coef_arrays) {
            std::cerr << CLI_RED << "Error: Failed to read JPEG coefficients in extract." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Extract metadata first (small, from first AC coefficients).
        auto meta_opt = this->dct_lsb_extract(coef_arrays, decompress.cinfo, sizeof(MetaData));
        if (!meta_opt || meta_opt->size() != sizeof(MetaData)) {
            std::cerr << CLI_RED << "Error: Failed to extract JPEG metadata." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }
        std::copy(meta_opt->begin(), meta_opt->end(), reinterpret_cast<byte*>(&this->embed_data->meta));

        // Validate extracted metadata.
        if (this->embed_data->meta.container != ContainerType::PHOTO ||
            this->embed_data->meta.ext != Extension::JPEG ||
            this->embed_data->meta.write_size < sizeof(MetaData)) {
            std::cerr << CLI_RED << "Error: Invalid extracted metadata for JPEG." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }

        // Now extract full data using write_size from metadata.
        uint64_t full_bytes = this->embed_data->meta.write_size;
        auto full_opt = this->dct_lsb_extract(coef_arrays, decompress.cinfo, full_bytes);
        if (!full_opt || full_opt->size() != full_bytes) {
            std::cerr << CLI_RED << "Error: Failed to extract full JPEG data." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }

        // Extract encrypted data (skip metadata prefix).
        uint64_t encrypt_bytes = full_bytes - sizeof(MetaData);
        std::vector<byte> encrypt_data(full_opt->begin() + sizeof(MetaData), full_opt->end());
        this->embed_data->encrypt_data = std::move(encrypt_data);

        // Decrypt.
        AES256Encryption::getInstance().set_key(this->embed_data->key);
        this->embed_data->plain_data = AES256Encryption::getInstance().decrypt(this->embed_data->encrypt_data);

        jpeg_finish_decompress(&decompress.cinfo);

        std::cout << CLI_GREEN << "Extracted " << this->embed_data->plain_data.size() << " bytes from JPEG DCT." << CLI_RESET << std::endl;
        return path;
    }

    std::optional<std::vector<byte>> PhotoHnS::dct_lsb_extract(jvirt_barray_ptr* coef_arrays,
                                                               const jpeg_decompress_struct& cinfo,
                                                               uint64_t num_bytes) const
    {
        if (num_bytes == 0) return std::vector<byte>{};
        uint64_t total_bits = num_bytes * 8ULL;
        std::vector<byte> data(num_bytes, 0);
        uint64_t bit_idx = 0;

        // Iteration same as embed: over components, block rows, blocks, AC coefficients.
        // Access virtual arrays row-by-row for reading (read mode: FALSE).
        for (int ci = 0; ci < cinfo.num_components && bit_idx < total_bits; ++ci) {
            jpeg_component_info* comp = cinfo.comp_info + ci;

            for (JDIMENSION blk_row = 0; blk_row < comp->height_in_blocks && bit_idx < total_bits; ++blk_row) {
                // Access one row of blocks (read-only); safety check for NULL.
                JBLOCKARRAY block_array = (JBLOCKARRAY) (*cinfo.mem->access_virt_barray)
                    ((j_common_ptr) &cinfo, coef_arrays[ci], blk_row, 1, FALSE);
                if (block_array == nullptr) {
                    std::cerr << CLI_RED << "Error: Failed to access DCT block row " << blk_row << " for extraction (component " << ci << ")." << CLI_RESET << std::endl;
                    return std::nullopt;  // Abort extraction safely.
                }
                JBLOCKROW block_row = block_array[0];  // Single row.

                for (JDIMENSION blk_col = 0; blk_col < comp->width_in_blocks && bit_idx < total_bits; ++blk_col) {
                    JBLOCK* block = block_row + blk_col;  // Pointer to JBLOCK (JCOEF[64]).

                    for (int k = 1; k < DCTSIZE2 && bit_idx < total_bits; ++k) {
                        // LSB from coefficient.
                        const JCOEF& coef = (*block)[k];
                        byte bit = static_cast<byte>(coef & 1);

                        uint64_t byte_idx = bit_idx / 8ULL;
                        if (byte_idx >= num_bytes) break;

                        int bit_offset = bit_idx % 8;
                        data[byte_idx] |= (bit << (7 - bit_offset));
                        ++bit_idx;
                    }
                }
            }
        }

        if (bit_idx < total_bits) {
            std::cerr << CLI_RED << "Error: Incomplete DCT extraction (" << bit_idx << "/" << total_bits << " bits)." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        return data;
    }

    std::optional<std::vector<byte>> PhotoHnS::extract(const std::string& path)
    {
        // Step 0: Initialize context (fail if none).
        if (!this->embed_data) {
            std::cerr << CLI_RED << "Error: No EmbedData context in extract." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Step 0.1: Set key (singleton, once — avoid duplicates).
        this->embed_data->key = AuthorKey::getInstance().get_key();

        // Step 0.2: Validate path and type (adaptive by ext).
        auto ext_opt = validate_path(path);
        if (!ext_opt) {
            std::cerr << CLI_RED << "Error: Invalid path for extraction: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        std::string filetype = ext_opt.value();
        bool is_jpeg = (filetype == "jpg" || filetype == "jpeg");
        bool skip_pixel_mode = is_jpeg;  // For JPEG: direct DCT, no stbi_load().

        // Step 1: Attempt pixel loading (only for PNG).
        struct StbiDeleter {
            void operator()(byte* p) const noexcept { stbi_image_free(p); }
        };
        std::unique_ptr<byte, StbiDeleter> image_guard;  // RAII: auto-free.
        int32_t width = 0, height = 0, channels = 0;
        byte* image = nullptr;
        uint64_t img_bytes = 0;
        bool is_loadable = false;

        if (!skip_pixel_mode) {  // PNG: Load pixels.
            StbImageUniquePtr img(stbi_load(path.c_str(), &width, &height, &channels, 0));
            if (!img) return std::nullopt;
            is_loadable = (image != nullptr);
            if (is_loadable) {
                image_guard.reset(image);
                img_bytes = static_cast<uint64_t>(width) * height * channels;
            }
        } else {
            is_loadable = false;  // For JPEG: Simulate fail, proceed to fallback.
        }

        if (!is_loadable) {
            // Silent for JPEG (no cerr); error only for PNG (real fail).
            if (!skip_pixel_mode) {
                std::cerr << CLI_RED << "Error: Failed to load image: " << path << " (stbi)." << CLI_RESET << std::endl;
                return std::nullopt;
            }
            // For JPEG: Quietly skip (fallback will handle).
        }

        // Step 2: Extract metadata (from pixels if loaded).
        MetaData extracted_meta{};  // Local for validation.
        bool meta_from_pixels = false;
        bool dct_fallback_used = false;  // Flag: fallback processed?
        if (is_loadable && img_bytes >= sizeof(MetaData) * 8ULL) {
            // LSB 1-bit from first bytes (MSB-first).
            size_t meta_byte_size = sizeof(MetaData);
            std::vector<byte> meta_bytes(meta_byte_size, 0);
            size_t bit_pos = 0;
            for (size_t i = 0; i < meta_byte_size * 8ULL; ++i) {
                if (i >= img_bytes) break;  // Bounds-check.

                byte bit = image[i] & 0x01;
                size_t byte_idx = bit_pos / 8;
                size_t bit_offset = bit_pos % 8;
                meta_bytes[byte_idx] |= (bit << (7 - bit_offset));
                ++bit_pos;
            }

            // Copy to extracted_meta.
            std::copy(meta_bytes.begin(), meta_bytes.end(), reinterpret_cast<byte*>(&extracted_meta));

            // Validation: If PHOTO — use pixels.
            if (extracted_meta.container == ContainerType::PHOTO) {
                meta_from_pixels = true;
                this->embed_data->meta = extracted_meta;
            } else {
                meta_from_pixels = false;
                std::cerr << CLI_YELLOW << "Pixel metadata invalid (not PHOTO), trying JPEG DCT..." << CLI_RESET << std::endl;
                image_guard.reset();  // Free pixels early.
            }
        } else if (!is_loadable) {
            meta_from_pixels = false;  // For JPEG: fallback.
        }

        // Step 3: Fallback to DCT for JPEG (if not meta_from_pixels).
        if (!meta_from_pixels) {
            dct_fallback_used = true;
            JpegDecompressRAII decompress;
            FILE* infile = std::fopen(path.c_str(), "rb");
            if (!infile) {
                std::cerr << CLI_RED << "Error: Failed to open for JPEG fallback: " << path << CLI_RESET << std::endl;
                return std::nullopt;
            }
            auto close_infile = [](FILE* f) { std::fclose(f); };
            std::unique_ptr<FILE, decltype(close_infile)> infile_guard(infile, close_infile);

            jpeg_stdio_src(&decompress.cinfo, infile);
            if (jpeg_read_header(&decompress.cinfo, TRUE) == JPEG_SUSPENDED) {
                std::cerr << CLI_RED << "Error: JPEG header suspended in fallback." << CLI_RESET << std::endl;
                return std::nullopt;
            }

            jvirt_barray_ptr* coef_arrays = jpeg_read_coefficients(&decompress.cinfo);
            if (!coef_arrays) {
                std::cerr << CLI_RED << "Error: Failed to read JPEG coefficients for fallback." << CLI_RESET << std::endl;
                return std::nullopt;
            }

            // Extract meta from DCT (1-bit LSB).
            auto meta_dct_opt = this->dct_lsb_extract(coef_arrays, decompress.cinfo, sizeof(MetaData));
            if (!meta_dct_opt || meta_dct_opt->size() != sizeof(MetaData)) {
                std::cerr << CLI_RED << "Error: Failed to extract JPEG metadata from DCT." << CLI_RESET << std::endl;
                jpeg_finish_decompress(&decompress.cinfo);
                return std::nullopt;
            }
            std::copy(meta_dct_opt->begin(), meta_dct_opt->end(), reinterpret_cast<byte*>(&this->embed_data->meta));

            // Validate DCT meta.
            if (this->embed_data->meta.container != ContainerType::PHOTO ||
                this->embed_data->meta.ext != Extension::JPEG ||
                this->embed_data->meta.write_size < sizeof(MetaData)) {
                std::cerr << CLI_RED << "Error: Invalid JPEG metadata from DCT." << CLI_RESET << std::endl;
                jpeg_finish_decompress(&decompress.cinfo);
                return std::nullopt;
            }

            // Full extraction from DCT.
            uint64_t full_bytes = this->embed_data->meta.write_size;
            auto full_dct_opt = this->dct_lsb_extract(coef_arrays, decompress.cinfo, full_bytes);
            if (!full_dct_opt || full_dct_opt->size() != full_bytes) {
                std::cerr << CLI_RED << "Error: Incomplete full extraction from JPEG DCT." << CLI_RESET << std::endl;
                jpeg_finish_decompress(&decompress.cinfo);
                return std::nullopt;
            }

            // Encrypt_data (skip meta).
            uint64_t encrypt_bytes = full_bytes - sizeof(MetaData);
            std::vector<byte> encrypt_data(full_dct_opt->begin() + sizeof(MetaData), full_dct_opt->end());
            this->embed_data->encrypt_data = std::move(encrypt_data);

            jpeg_finish_decompress(&decompress.cinfo);

            // Decrypt (key already set).
            AES256Encryption::getInstance().set_key(this->embed_data->key);
            this->embed_data->plain_data = AES256Encryption::getInstance().decrypt(this->embed_data->encrypt_data);

            std::cout << CLI_GREEN << "Extracted " << this->embed_data->plain_data.size() << " bytes from JPEG DCT." << CLI_RESET << std::endl;
            return this->embed_data->plain_data;
        }

        // Step 4: Meta valid from pixels — branch by ext (switch for symmetry).
        if (dct_fallback_used) {
            return this->embed_data->plain_data;  // Fallback already processed.
        }
        std::optional<std::string> result;
        switch (this->embed_data->meta.ext) {
            case Extension::PNG:
                result = png_out(image, this->embed_data->meta, path);
                break;
            case Extension::JPEG:
                // Rare case: JPEG with valid pixel meta — fallback to DCT.
                std::cerr << CLI_YELLOW << "Warning: JPEG detected via pixels; using DCT fallback." << CLI_RESET << std::endl;
                image_guard.reset();
                result = jpg_out(path);
                break;
            default:
                std::cerr << CLI_YELLOW << "Warning: Unsupported extension in metadata." << CLI_RESET << std::endl;
                return std::nullopt;
        }

        if (!result.has_value()) {
            return std::nullopt;
        }

        // Step 5: Decrypt (for PNG, key already set).
        AES256Encryption::getInstance().set_key(this->embed_data->key);
        this->embed_data->plain_data = AES256Encryption::getInstance().decrypt(this->embed_data->encrypt_data);

        std::cout << CLI_GREEN << "Extracted " << this->embed_data->plain_data.size() << " bytes from PNG pixels." << CLI_RESET << std::endl;
        return this->embed_data->plain_data;
    }

    // ---------------------------------------------------------------------
    // Fast metadata detection (tryReadMetaOnly)
    // ---------------------------------------------------------------------
    std::optional<MetaData> PhotoHnS::tryReadMetaOnly(const std::string& path)
    {
        const auto ext_opt = HnS::validate_path(path);
        if (!ext_opt) return std::nullopt;
        const std::string ext = ext_opt.value();

        if (ext == "png") {
            int w, h, ch;
            StbImageUniquePtr img(stbi_load(path.c_str(), &w, &h, &ch, 0));
            if (!img) return std::nullopt;

            const uint64_t total_bytes = static_cast<uint64_t>(w) * h * ch;

            for (int mode = 0; mode < 2; ++mode) {
                std::vector<byte> buffer(sizeof(MetaData), 0);
                uint64_t bit_pos = 0;

                for (uint64_t i = 0; i < total_bytes && bit_pos < sizeof(MetaData) * 8; ++i) {
                    byte bit = img[i] & 1;
                    uint64_t byte_idx = bit_pos / 8;
                    uint64_t bit_idx = 7 - (bit_pos % 8);
                    buffer[byte_idx] |= (bit << bit_idx);
                    ++bit_pos;
                }

                MetaData meta{};
                std::memcpy(&meta, buffer.data(), sizeof(MetaData));

                if (meta.container == ContainerType::PHOTO &&
                    meta.ext == Extension::PNG &&
                    meta.write_size >= sizeof(MetaData)) {
                    meta.lsb_mode = mode ? LsbMode::TwoBits : LsbMode::OneBit;
                    return meta;
                }
            }
        }

        if (ext == "jpg" || ext == "jpeg") {
            JpegDecompressRAII d;
            FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) return std::nullopt;
            auto close_f = [](FILE* fp) { if (fp) std::fclose(fp); };
            std::unique_ptr<FILE, decltype(close_f)> fg(f, close_f);

            jpeg_stdio_src(&d.cinfo, f);
            jpeg_read_header(&d.cinfo, TRUE);
            jvirt_barray_ptr* coefs = jpeg_read_coefficients(&d.cinfo);
            if (!coefs) return std::nullopt;

            auto extracted = dct_lsb_extract(coefs, d.cinfo, sizeof(MetaData));
            jpeg_finish_decompress(&d.cinfo);

            if (extracted && extracted->size() == sizeof(MetaData)) {
                MetaData meta{};
                std::memcpy(&meta, extracted->data(), sizeof(MetaData));
                if (meta.container == ContainerType::PHOTO && meta.ext == Extension::JPEG) {
                    meta.lsb_mode = LsbMode::OneBit;
                    return meta;
                }
            }
        }

        return std::nullopt;
    }

} // Yps