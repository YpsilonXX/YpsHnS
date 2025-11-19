#include "HnS.hh"
#include "PhotoHnS/PhotoHnS.hh"
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

std::optional<MetaData> HnS::readMetaOnly(const std::string& path)
{
    auto ext_opt = validate_path(path);
    if (!ext_opt) {
        return std::nullopt;
    }
    const std::string ext = ext_opt.value();

    // PHOTO — поддерживаем PNG и JPEG
    if (ext == "png" || ext == "jpg" || ext == "jpeg") {
        PhotoHnS photo;
        return photo.tryReadMetaOnly(path);
    }

    // VIDEO — в будущем
    // if (ext == "mp4" || ext == "mkv" || ext == "avi") {
    //     VideoHnS video;
    //     return video.tryReadMetaOnly(path);
    // }

    // AUDIO — в будущем
    // if (ext == "wav" || ext == "flac" || ext == "mp3") {
    //     AudioHnS audio;
    //     return audio.tryReadMetaOnly(path);
    // }

    return std::nullopt;
}


} // Yps