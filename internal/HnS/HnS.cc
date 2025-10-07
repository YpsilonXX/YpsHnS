#include "HnS.hh"

#include <iostream>

namespace Yps
{


std::optional<std::string> HnS::validate_path(const std::string& path)
{
    /*Use it only inside method*/
    namespace fs = std::filesystem;

    /*Empty check*/
    if (path.empty())
        return std::nullopt;

    try
    {
        fs::path f_path(path);
        /*Check existence and directory*/
        if (!fs::exists(f_path) || fs::is_directory(f_path))
            return std::nullopt;

        std::string extension = f_path.extension().string();
        if (!extension.empty() && extension[0] == '.') {
            extension.erase(0, 1);
        }

        return extension;
    } catch (const fs::filesystem_error& e)
    {
        std::cerr << "Error while HnS::validate_path: " << e.what() << std::endl;
        return std::nullopt;
    }

}



} // Yps