// PhotoHnS.hh
#ifndef YPSHNS_PHOTOHNS_HH
#define YPSHNS_PHOTOHNS_HH

#include <memory>
#include <HnS.hh>
#include <Encryption.hh>
#include <EmbedData.hh>
#include <jpeglib.h>  // libjpeg-turbo
#include <array>
#include <algorithm>  // For std::clamp
#include <iomanip>    // For std::hex in debug

namespace Yps
{
    // RAII for jpeg_decompress_struct (auto-cleanup).
    class JpegDecompressRAII {
    public:
        jpeg_decompress_struct cinfo;
        jpeg_error_mgr jerr;
        explicit JpegDecompressRAII() noexcept {
            jpeg_create_decompress(&cinfo);
            cinfo.err = jpeg_std_error(&jerr);
        }
        ~JpegDecompressRAII() noexcept { jpeg_destroy_decompress(&cinfo); }
        JpegDecompressRAII(const JpegDecompressRAII&) = delete;
        JpegDecompressRAII& operator=(const JpegDecompressRAII&) = delete;
    };

    // RAII for jpeg_compress_struct.
    class JpegCompressRAII {
    public:
        jpeg_compress_struct cinfo;
        jpeg_error_mgr jerr;
        explicit JpegCompressRAII() noexcept {
            jpeg_create_compress(&cinfo);
            cinfo.err = jpeg_std_error(&jerr);
        }
        ~JpegCompressRAII() noexcept { jpeg_destroy_compress(&cinfo); }
        JpegCompressRAII(const JpegCompressRAII&) = delete;
        JpegCompressRAII& operator=(const JpegCompressRAII&) = delete;
    };

    class PhotoHnS : public HnS
    {
    private:
        /**
         * Check alpha channel in PNG: Full opacity (255) to embed without artifacts.
         * @param image Raw image bytes.
         * @param width/height/channels Dimensions.
         * @return true if alpha is usable (all 255).
         */
        static bool has_usable_alpha(const byte* image, int32_t width, int32_t height, int32_t channels);

        /**
         * Embed into PNG: LSB in pixels (1/2 bits per byte).
         * @param path Input file.
         * @param out_path Output file.
         * @param encrypted_data Encrypted data.
         * @param meta Metadata (updated with mode).
         * @return out_path or nullopt on failure.
         */
        std::optional<std::string> png_embed(const std::string& path, const std::string& out_path,
                                             const std::vector<byte>& encrypted_data, MetaData& meta);

        /**
         * Embed into JPEG: LSB in AC-DCT coefficients (low-freq, robust to re-compression).
         * @param path Input file.
         * @param out_path Output file.
         * @param encrypted_data Encrypted data.
         * @param meta Metadata.
         * @return out_path or nullopt on failure.
         */
        std::optional<std::string> jpg_embed(const std::string& path, const std::string& out_path,
                                             const std::vector<byte>& encrypted_data, const MetaData& meta);

        /**
         * Extract from JPEG: LSB from AC-DCT coefficients.
         * @param path Input file.
         * @param key Decryption key.
         * @return Plain data or nullopt on failure.
         */
        std::optional<std::vector<byte>> jpg_extract(const std::string& path,
                                                     const std::array<byte, SHA256_DIGEST_LENGTH>& key);

        /**
         * Extract metadata from pixels (try 1-bit, then 2-bit mode).
         * @param image Image bytes.
         * @param img_bytes Size in bytes.
         * @return MetaData or nullopt if not found.
         */
        std::optional<MetaData> extract_meta_from_pixels(const byte* image, uint64_t img_bytes);

        /**
         * Extract data from pixels based on mode.
         * @param image Image bytes.
         * @param img_bytes Size in bytes.
         * @param data_bytes Expected size.
         * @param mode LSB mode.
         * @return Full data or nullopt if incomplete.
         */
        std::optional<std::vector<byte>> extract_data_from_pixels(const byte* image, uint64_t img_bytes,
                                                                  uint64_t data_bytes, LsbMode mode);

        /**
         * LSB 1-bit per image byte (MSB-first, for PNG pixels).
         * @param image Modified in-place.
         * @param data Data to embed (const-ref).
         * @param img_bytes Bounds for safety.
         */
        void lsb_one_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes);

        /**
         * LSB 2-bits per byte (for PNG capacity; meta in 1-bit if mixed).
         * @param image Modified in-place.
         * @param data Data.
         * @param img_bytes Bounds.
         */
        void lsb_two_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes);

        /**
         * LSB extract 1-bit.
         * @param image Bytes.
         * @param data_bytes Expected size.
         * @param img_bytes Bounds.
         * @return Data or nullopt if incomplete.
         */
        std::optional<std::vector<byte>> lsb_extract_one_bit(const byte* image, uint64_t data_bytes, uint64_t img_bytes) const;

        /**
         * LSB extract 2-bits.
         * @param image Bytes.
         * @param data_bytes Expected size.
         * @param img_bytes Bounds.
         * @return Data or nullopt if incomplete.
         */
        std::optional<std::vector<byte>> lsb_extract_two_bit(const byte* image, uint64_t data_bytes, uint64_t img_bytes) const;

        /**
         * DCT-LSB embed: 1-bit in low-freq AC coefficients (skip DC).
         * @param coef_arrays DCT blocks.
         * @param cinfo Decompress info for loops.
         * @param data Data (meta + encrypted).
         */
        void dct_lsb_embed(jvirt_barray_ptr* coef_arrays, const jpeg_decompress_struct& cinfo,
                           const std::vector<byte>& data);

        /**
         * DCT-LSB extract: 1-bit from AC coefficients (MSB-first).
         * @param coef_arrays DCT blocks.
         * @param cinfo Decompress info.
         * @param data_bytes Expected size (meta + encrypted).
         * @return Full data or nullopt if incomplete.
         */
        std::optional<std::vector<byte>> dct_lsb_extract(jvirt_barray_ptr* coef_arrays,
                                                         const jpeg_decompress_struct& cinfo,
                                                         uint64_t data_bytes) const;

    public:
        ~PhotoHnS() = default;
        PhotoHnS() = default;

        /**
         * Embed data into photo (PNG/JPEG auto-detect).
         * @param data Data to hide.
         * @param path Input photo.
         * @param out_path Output (modified).
         * @return out_path or nullopt on failure (invalid path/capacity).
         */
        std::optional<std::string> embed(const std::vector<byte>& data, const std::string& path,
                                         const std::string& out_path) override;

        /**
         * Extract data from photo (PNG/JPEG based on meta).
         * @param path File with embedded data.
         * @return Plain data or nullopt on failure (no meta/invalid).
         */
        std::optional<std::vector<byte>> extract(const std::string& path) override;
    };
} // Yps

#endif //YPSHNS_PHOTOHNS_HH