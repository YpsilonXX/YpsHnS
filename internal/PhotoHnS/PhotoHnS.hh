#ifndef YPSHNS_PHOTOHNS_HH
#define YPSHNS_PHOTOHNS_HH

#include <memory>
#include <HnS.hh>
#include <EmbedData.hh>
#include <Encryption.hh>

namespace Yps
{
    class PhotoHnS : public HnS
    {
    private:
        /**
         * Check for available alpha for embed
         * @param image bytes of image
         * @param width image's width
         * @param height image's height
         * @param channels Count of channels
         * @return True - if usable alpha exist
         */
        static bool has_usable_alpha(const byte* image, int32_t width, int32_t height, int32_t channels);

        /**
         * Technical embed into PNG. Used EmbedData
         * @param out_path Path to output file
         * @return out_path or std::nullopt if failed
         */
        std::optional<std::string> png_in(const std::string& out_path);

        /**
         * Technical extract from PNG. Used EmbedData and sets plain_data if success.
         * @param image loaded image bytes
         * @param meta extracted metadata
         * @param path input path (for logging)
         * @return path if success, nullopt otherwise
         */
        std::optional<std::string> png_out(byte* image, MetaData& meta, const std::string& path);

        /**
         * Pack data to image using 1 LSB bit per image byte (MSB-first bit order).
         * @param image image's data (modified in place)
         * @param data data to embed (const, for safety)
         * @param img_bytes total image bytes (for bound check)
         */
        void lsb_one_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes);

        /**
         * Pack data to image using 2 LSB bits per image byte (MSB-first, packed pairs).
         * First part (meta) uses 1-bit mode to avoid overflow.
         * @param image image's data (modified in place)
         * @param data data to embed (const, for safety)
         * @param img_bytes total image bytes (for bound check)
         */
        void lsb_two_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes);

        std::unique_ptr<EmbedData> embed_data;
    public:
        ~PhotoHnS() = default;
        PhotoHnS() = default;

        /**
        * Embed data into Photo
        * @param data Vector with data to embed
        * @param path Path to input file
        * @param out_path Path to output file
        * @return out_path or std::nullopt if failed
        */
        std::optional<std::string> embed(const std::vector<byte>& data, const std::string& path, const std::string& out_path) override;

        /**
         * Extract data from Photo.
         * @param path Path to Photo file with embedded data
         * @return Vector with extracted bytes or std::nullopt if failed
         */
        std::optional<std::vector<byte>> extract(const std::string& path) override;

    };
} // Yps

#endif //YPSHNS_PHOTOHNS_HH