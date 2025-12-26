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
        // Initialize EmbedData (reset if needed).
        if (!this->embed_data)
            this->embed_data = std::make_unique<EmbedData>();
        else
            this->embed_data = std::make_unique<EmbedData>();  // Automatic reset via move.

        // Fill metadata (only filename, not full path — safer).
        this->embed_data->plain_data = data;
        this->embed_data->meta.container = ContainerType::PHOTO;
        this->embed_data->meta.filename = std::filesystem::path(path).filename().string();
        this->embed_data->key = AuthorKey::getInstance().get_key();

        // Encryption (key from AuthorKey).
        AES256Encryption::getInstance().set_key(this->embed_data->key);
        this->embed_data->encrypt_data = AES256Encryption::getInstance().encrypt(this->embed_data->plain_data);

        // write_size: encrypt + sizeof(MetaData) (temporary; for raw copy).
        this->embed_data->meta.write_size = this->embed_data->encrypt_data.size() + sizeof(MetaData);

        // Path validation.
        auto ext_opt = validate_path(path);
        if (!ext_opt) {
            std::cerr << CLI_RED << "PhotoHnS::embed(): Invalid path: " << path << CLI_RESET << std::endl;
            return std::nullopt;
        }
        std::string filetype = ext_opt.value();

        // Support PNG and JPEG.
        if (filetype == "png") {
            this->embed_data->meta.ext = Extension::PNG;
            this->embed_data->meta.lsb_mode = LsbMode::NoUsed;  // Will be set in png_in.
            return this->png_in(out_path);
        } else if (filetype == "jpg" || filetype == "jpeg") {
            this->embed_data->meta.ext = Extension::JPEG;
            this->embed_data->meta.lsb_mode = LsbMode::OneBit;  // Only 1-bit mode for DCT.
            return this->jpg_in(out_path);
        }

        std::cerr << CLI_RED << "PhotoHnS::embed(): Unsupported extension: " << filetype << CLI_RESET << std::endl;
        return std::nullopt;
    }

    std::optional<std::string> PhotoHnS::png_in(const std::string &out_path)
    {
        // Load image (RAII: free at end).
        int32_t width, height, channels;
        byte* image = stbi_load(this->embed_data->meta.filename.c_str(), &width, &height, &channels, 0);
        if (!image) {
            std::cerr << CLI_RED << "Error: Failed to load PNG: " << this->embed_data->meta.filename << CLI_RESET << std::endl;
            return std::nullopt;
        }

        auto free_image = [](byte* p) noexcept { stbi_image_free(p); };
        std::unique_ptr<byte, decltype(free_image)> image_guard(image, free_image);

        // Capacity calculation (image bytes = bits for 1-bit LSB).
        uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;
        uint64_t data_bytes = this->embed_data->encrypt_data.size() + sizeof(MetaData);
        uint64_t total_bits = data_bytes * 8ULL;

        // Mode selection (strict <= for safety).
        LsbMode mode = LsbMode::NoUsed;
        if (total_bits <= img_bytes) {
            mode = LsbMode::OneBit;
        } else if (total_bits <= img_bytes * 2ULL) {  // Simplified; optionally subtract meta*8.
            std::cout << CLI_YELLOW << "Warning: Using LsbMode::TwoBits — artifacts may be visible." << CLI_RESET << std::endl;
            mode = LsbMode::TwoBits;
        } else {
            std::cerr << CLI_RED << "Error: Insufficient capacity in PNG (needed " << total_bits
                      << " bits, available ~" << img_bytes * 2 << ")." << CLI_RESET << std::endl;
            return std::nullopt;
        }
        this->embed_data->meta.lsb_mode = mode;

        // Prepare full_data: metadata followed by encrypted data.
        std::vector<byte> full_data(data_bytes);
        MetaData* m = &this->embed_data->meta;
        std::copy(reinterpret_cast<const byte*>(m), reinterpret_cast<const byte*>(m) + sizeof(MetaData), full_data.begin());
        std::copy(this->embed_data->encrypt_data.begin(), this->embed_data->encrypt_data.end(),
                  full_data.begin() + sizeof(MetaData));

        // Embedding with bounds checks.
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

        // Save (stride=0 auto).
        int success = stbi_write_png(out_path.c_str(), width, height, channels, image, 0);
        if (!success) {
            std::cerr << CLI_RED << "Error: Failed to write PNG: " << out_path << CLI_RESET << std::endl;
            return std::nullopt;
        }

        std::cout << CLI_GREEN << "Embedded " << data_bytes << " bytes into " << out_path << " (mode: "
                  << static_cast<int>(mode) << ")." << CLI_RESET << std::endl;
        return out_path;
    }

    std::optional<std::string> PhotoHnS::jpg_in(const std::string &out_path)
    {
        // Prepare full_data: metadata + encrypted data.
        uint64_t data_bytes = this->embed_data->meta.write_size;
        std::vector<byte> full_data(data_bytes);
        MetaData* m = &this->embed_data->meta;
        std::copy(reinterpret_cast<const byte*>(m), reinterpret_cast<const byte*>(m) + sizeof(MetaData), full_data.begin());
        std::copy(this->embed_data->encrypt_data.begin(), this->embed_data->encrypt_data.end(),
                  full_data.begin() + sizeof(MetaData));

        uint64_t total_bits = data_bytes * 8ULL;
        if (total_bits == 0) return std::nullopt;  // Edge case.

        // RAII for decompress (manual finish — see below).
        JpegDecompressRAII decompress;
        FILE* infile = std::fopen(this->embed_data->meta.filename.c_str(), "rb");
        if (!infile) {
            std::cerr << CLI_RED << "Error: Failed to open JPEG: " << this->embed_data->meta.filename << CLI_RESET << std::endl;
            return std::nullopt;
        }
        auto close_infile = [](FILE* f) { std::fclose(f); };
        std::unique_ptr<FILE, decltype(close_infile)> infile_guard(infile, close_infile);

        // Set up input source and read header.
        jpeg_stdio_src(&decompress.cinfo, infile);
        if (jpeg_read_header(&decompress.cinfo, TRUE) == JPEG_SUSPENDED) {
            std::cerr << CLI_RED << "Error: JPEG header read suspended." << CLI_RESET << std::endl;
            return std::nullopt;
        }

        // Log progressive and force baseline (enforced on compress side).
        if (decompress.cinfo.progressive_mode) {
            std::cout << CLI_YELLOW << "Input is progressive JPEG; forcing baseline output." << CLI_RESET << std::endl;
        }

        // Read DCT coefficients (keep decompress alive until transcoding end).
        jvirt_barray_ptr* coef_arrays = jpeg_read_coefficients(&decompress.cinfo);
        if (!coef_arrays) {
            std::cerr << CLI_RED << "Error: Failed to read JPEG coefficients." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);  // Safe cleanup.
            return std::nullopt;
        }

        // Calculate capacity (AC: 63 bits per block, skip DC).
        uint64_t ac_capacity_bits = 0;
        for (int ci = 0; ci < decompress.cinfo.num_components; ++ci) {
            jpeg_component_info* comp = decompress.cinfo.comp_info + ci;
            ac_capacity_bits += static_cast<uint64_t>(comp->height_in_blocks) * comp->width_in_blocks * 63ULL;
        }
        if (total_bits > ac_capacity_bits) {
            std::cerr << CLI_RED << "Error: Insufficient capacity in JPEG (needed " << total_bits
                      << " bits, available " << ac_capacity_bits << ")." << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);
            return std::nullopt;
        }
        std::cout << CLI_YELLOW << "JPEG capacity check: " << ac_capacity_bits << " AC bits available." << CLI_RESET << std::endl;

        // Embed LSB in AC (modifies coef_arrays via access_virt_barray).
        this->dct_lsb_embed(coef_arrays, decompress.cinfo, full_data);

        // RAII for compress (manual finish below).
        JpegCompressRAII compress;
        FILE* outfile = std::fopen(out_path.c_str(), "wb");
        if (!outfile) {
            std::cerr << CLI_RED << "Error: Failed to open output: " << out_path << CLI_RESET << std::endl;
            jpeg_finish_decompress(&decompress.cinfo);  // Cleanup on fail.
            return std::nullopt;
        }
        auto close_outfile = [](FILE* f) { std::fclose(f); };
        std::unique_ptr<FILE, decltype(close_outfile)> outfile_guard(outfile, close_outfile);

        jpeg_stdio_dest(&compress.cinfo, outfile);

        // Copy critical parameters — do this while decompress is still valid.
        jpeg_copy_critical_parameters(&decompress.cinfo, &compress.cinfo);

        // Force baseline to avoid Huffman corruption (no progressive/arith).
        compress.cinfo.progressive_mode = FALSE;
        compress.cinfo.arith_code = FALSE;
        compress.cinfo.optimize_coding = FALSE;  // Stable Huffman tables.

        // Pass modified coef_arrays to write_coefficients (no field assign).
        jpeg_write_coefficients(&compress.cinfo, coef_arrays);  // Full call: cinfo + arrays.

        // Finish compress (writes trailer, but does not free arrays — shared).
        jpeg_finish_compress(&compress.cinfo);  // Errors via err_mgr.

        // Now safe to finish decompress (free arrays after compress).
        jpeg_finish_decompress(&decompress.cinfo);

        std::cout << CLI_GREEN << "Embedded " << data_bytes << " bytes into JPEG DCT (" << out_path << ")." << CLI_RESET << std::endl;
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
    // Always reset EmbedData for each extraction to avoid carrying over context from previous operations.
    this->embed_data = std::make_unique<EmbedData>();

    this->embed_data->key = AuthorKey::getInstance().get_key();

    // Path validation.
    auto ext_opt = validate_path(path);
    if (!ext_opt) {
        std::cerr << CLI_RED << "PhotoHnS::extract(): Invalid path: " << path << CLI_RESET << std::endl;
        return std::nullopt;
    }
    std::string filetype = ext_opt.value();

    // Load image (RAII: free at end).
    int32_t width, height, channels;
    byte* image = stbi_load(path.c_str(), &width, &height, &channels, 0);
    bool is_loadable = (image != nullptr);
    auto free_image = [](byte* p) noexcept { stbi_image_free(p); };
    std::unique_ptr<byte, decltype(free_image)> image_guard(image, free_image);

    // Step 1: Assume PNG pixels first (extract meta with 1-bit LSB).
    bool meta_from_pixels = false;
    bool dct_fallback_used = false;
    if (is_loadable) {
        uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;

        // Extract meta from pixels (always 1-bit for meta).
        auto meta_opt = this->lsb_extract_meta(image, img_bytes);
        if (meta_opt && meta_opt->size() == sizeof(MetaData)) {
            std::copy(meta_opt->begin(), meta_opt->end(), reinterpret_cast<byte*>(&this->embed_data->meta));

            // Validate meta.
            if (this->embed_data->meta.container == ContainerType::PHOTO &&
                (this->embed_data->meta.ext == Extension::PNG || this->embed_data->meta.ext == Extension::JPEG) &&
                this->embed_data->meta.write_size > sizeof(MetaData) &&
                this->embed_data->meta.meta_size == sizeof(MetaData)) {
                meta_from_pixels = true;
                std::cout << CLI_GREEN << "Valid metadata from pixels (size: " << this->embed_data->meta.write_size << ")." << CLI_RESET << std::endl;
            } else {
                std::cerr << CLI_YELLOW << "Pixel metadata invalid (not PHOTO), trying JPEG DCT..." << CLI_RESET << std::endl;
                image_guard.reset();  // Free pixels early if not needed.
            }
        } else {
            std::cerr << CLI_YELLOW << "Failed to extract metadata from pixels, trying JPEG DCT..." << CLI_RESET << std::endl;
        }
    }

    // Step 2: Fallback to DCT for JPEG (if not meta_from_pixels).
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

    // Step 3: Meta valid from pixels — extract full data based on mode.
    uint64_t encrypt_bytes = this->embed_data->meta.write_size - sizeof(MetaData);
    uint64_t encrypt_bits = encrypt_bytes * 8;

    // Skip bytes used for meta (1-bit LSB, so 8 bits per byte for meta -> sizeof(MetaData)*8 bytes skipped).
    uint64_t meta_bits = sizeof(MetaData) * 8;
    uint64_t img_bytes = static_cast<uint64_t>(width) * height * channels;
    if (img_bytes < meta_bits) {
        std::cerr << CLI_RED << "Error: Insufficient image size for meta." << CLI_RESET << std::endl;
        return std::nullopt;
    }
    byte* data_image = image + meta_bits;
    uint64_t data_img_bytes = img_bytes - meta_bits;

    std::vector<byte> encrypt_data;
    switch (this->embed_data->meta.lsb_mode) {
        case LsbMode::OneBit:
            {
                auto opt = lsb_one_bit_extract(data_image, data_img_bytes, encrypt_bytes);
                if (!opt) {
                    std::cerr << CLI_RED << "Error: Insufficient bits for 1-bit extraction." << CLI_RESET << std::endl;
                    return std::nullopt;
                }
                encrypt_data = opt.value();
            }
            break;
        case LsbMode::TwoBits:
            {
                auto opt = lsb_two_bit_extract(data_image, data_img_bytes, encrypt_bytes);
                if (!opt) {
                    std::cerr << CLI_RED << "Error: Insufficient bits for 2-bit extraction." << CLI_RESET << std::endl;
                    return std::nullopt;
                }
                encrypt_data = opt.value();
            }
            break;
        default:
            std::cerr << CLI_RED << "Error: Unsupported LSB mode." << CLI_RESET << std::endl;
            return std::nullopt;
    }

    this->embed_data->encrypt_data = std::move(encrypt_data);

    // Step 4: Decrypt.
    AES256Encryption::getInstance().set_key(this->embed_data->key);
    this->embed_data->plain_data = AES256Encryption::getInstance().decrypt(this->embed_data->encrypt_data);

    std::cout << CLI_GREEN << "Extracted " << this->embed_data->plain_data.size() << " bytes from pixels." << CLI_RESET << std::endl;
    return this->embed_data->plain_data;
}

} // Yps