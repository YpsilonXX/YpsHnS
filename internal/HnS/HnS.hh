#ifndef YPSHNS_HNS_HH
#define YPSHNS_HNS_HH

#include <optional>
#include <string>
#include <vector>
#include <filesystem>

#include <defines.hh>

namespace Yps
{
    /**Interface for embed data*/
    class HnS
    {
    protected:
        /**
         * Check path for validity.
         * @param path Path to file
         * @return file's extension (jpg, wav, etc) or std::nullopt
         */
        static std::optional<std::string> validate_path(const std::string& path);

    public:
        /**Correct delete for children*/
        virtual ~HnS() = default;

        /**
         * Embed data to some container
         * @param data Vector with data to embed
         * @param path Path to file's container
         * @param out_path  Path to modified file
         * @return  out_path or std::nullopt, if failed
         */
        virtual std::optional<std::string> embed(const std::vector<byte>& data, const std::string& path, const std::string& out_path) = 0;

        /**
         * Get data from modified file
         * @param path Path to file with container
         * @return vector with bytes or std::nullopt, if failed
         */
        virtual std::optional<std::vector<byte>> extract(const std::string& path) = 0;
    };
} // Yps

#endif //YPSHNS_HNS_HH