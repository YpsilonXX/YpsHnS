#ifndef YPSHNS_PHOTOHNS_HH
#define YPSHNS_PHOTOHNS_HH

#include <HnS.hh>
#include "jpg.hh"

namespace Yps
{
    enum class LsbMode {
        OneBit,
        TwoBits
    };

    class PhotoHnS : public HnS
    {
    private:

    public:
        ~PhotoHnS() = default;
        PhotoHnS() = default;

        /**
        * Embed data into PNG
        * @param data Vector with data to embed
        * @param path Path to input PNG file
        * @param out_path Path to output PNG file
        * @return out_path or std::nullopt if failed
        */
        std::optional<std::string> embed(const std::vector<byte>& data, const std::string& path, const std::string& out_path) override;

        /**
         * Extract data from PNG.
         * @param path Path to PNG file with embedded data
         * @return Vector with extracted bytes or std::nullopt if failed
         */
        std::optional<std::vector<byte>> extract(const std::string& path) override;

    };
} // Yps

#endif //YPSHNS_PHOTOHNS_HH