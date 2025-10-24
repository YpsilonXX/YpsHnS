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
         * @return out_path or std::nullptr if failed
         */
        std::optional<std::string> png_in(const std::string& out_path);

        std::optional<std::string> png_out(byte* image, uint64_t img_size, MetaData& meta, const std::string& path);

        /**
         *  Pack data to image
         * @param image image's data
         * @param data data to embed
         */
        static void lsb_one_bit(byte* image, std::vector<byte>& data);
        /**
         *  Pack data to image
         * @param image image's data
         * @param data data to embed
         */
        void lsb_two_bit(byte* image, std::vector<byte>& data);

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