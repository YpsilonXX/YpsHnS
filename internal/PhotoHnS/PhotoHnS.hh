#ifndef YPSHNS_PHOTOHNS_HH
#define YPSHNS_PHOTOHNS_HH

#include <memory>
#include <HnS.hh>
#include <EmbedData.hh>
#include <Encryption.hh>
#include <jpeglib.h>  // libjpeg-turbo
#include <array>
#include <algorithm>  // Для std::clamp
#include <iomanip>    // Для std::hex в debug

namespace Yps
{
    // RAII для jpeg_decompress_struct (авто-cleanup).
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

    // RAII для jpeg_compress_struct.
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
         * Проверка альфа-канала в PNG: Полная непрозрачность (255) для встраивания без артефактов.
         * @param image Сырые байты изображения.
         * @param width/height/ channels Размеры.
         * @return true, если альфа usable (все 255).
         */
        static bool has_usable_alpha(const byte* image, int32_t width, int32_t height, int32_t channels);

        /**
         * Embed в PNG: LSB в пикселях (1/2 бита на байт).
         * @param out_path Выходной файл.
         * @return out_path или nullopt (fail).
         */
        std::optional<std::string> png_in(const std::string& out_path);

        /**
         * Extract из PNG: LSB из пикселей.
         * @param image Загруженные байты (stb).
         * @param meta Извлечённые метаданные.
         * @param path Для логов.
         * @return path или nullopt.
         */
        std::optional<std::string> png_out(byte* image, MetaData& meta, const std::string& path);

        /**
         * Embed в JPEG: LSB в AC-DCT-коэффициентах (low-freq, robust to re-compress).
         * @param out_path Выходной файл.
         * @return out_path или nullopt.
         */
        std::optional<std::string> jpg_in(const std::string& out_path);

        /**
         * Extract из JPEG: LSB из AC-DCT-коэффициентов.
         * @param path Входной файл (direct DCT-access).
         * @return path или nullopt.
         */
        std::optional<std::string> jpg_out(const std::string& path);

        /**
         * LSB 1-бит на байт изображения (MSB-first, для PNG pixels).
         * @param image Модифицируется in-place.
         * @param data Данные для embed (const-ref).
         * @param img_bytes Общий размер для bounds.
         */
        void lsb_one_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes);

        /**
         * LSB 2-бита на байт (meta в 1-bit, остальное 2-bit; для PNG ёмкости).
         * @param image Модифицируется in-place.
         * @param data Данные.
         * @param img_bytes Bounds.
         */
        void lsb_two_bit(byte* image, const std::vector<byte>& data, uint64_t img_bytes);

        /**
         * DCT-LSB embed: 1-бит в low-freq AC-коэффициентах (skip DC).
         * @param coef_arrays DCT-блоки (jvirt_barray_ptr*).
         * @param cinfo Decompress info (для loops).
         * @param data Данные (meta + encrypt).
         */
        void dct_lsb_embed(jvirt_barray_ptr* coef_arrays, const jpeg_decompress_struct& cinfo,
                           const std::vector<byte>& data);

        /**
         * DCT-LSB extract: 1-бит из AC-коэффициентов (MSB-first).
         * @param coef_arrays DCT-блоки.
         * @param cinfo Decompress info.
         * @param data_bytes Ожидаемый размер (meta + encrypt).
         * @return full_data или nullopt (incomplete).
         */
        std::optional<std::vector<byte>> dct_lsb_extract(jvirt_barray_ptr* coef_arrays,
                                                        const jpeg_decompress_struct& cinfo,
                                                        uint64_t data_bytes) const;


    public:
        ~PhotoHnS() = default;
        PhotoHnS() = default;
        std::unique_ptr<EmbedData> embed_data;  // Контекст: plain/encrypt/meta/key.
        /**
         * Embed данных в фото (PNG/JPEG auto-detect).
         * @param data Данные для скрытия.
         * @param path Входное фото.
         * @param out_path Выходное (модифицированное).
         * @return out_path или nullopt (fail: invalid path/capacity).
         */
        std::optional<std::string> embed(const std::vector<byte>& data, const std::string& path,
                                         const std::string& out_path) override;

        /**
         * Extract данных из фото (PNG/JPEG по meta).
         * @param path Файл с embedded данными.
         * @return plain_data или nullopt (fail: no meta/invalid).
         */
        std::optional<std::vector<byte>> extract(const std::string& path) override;

        std::optional<MetaData> tryReadMetaOnly(const std::string& path) override;
    };
} // Yps

#endif //YPSHNS_PHOTOHNS_HH