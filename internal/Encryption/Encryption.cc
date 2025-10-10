//
// Created by ypsilon on 10/7/25.
//

#include <utility>

#include "Encryption.hh"

namespace Yps
{
    std::vector<byte> Encryption::encrypt(const std::vector<byte>& data)
    {
        return data;
    }

    std::vector<byte> Encryption::decrypt(const std::vector<byte>& data)
    {
        return data;
    }

    Encryption &Encryption::getInstance()
    {
        static Encryption instance;
        return instance;
    }





    AES256Encryption::AES256Encryption()
    {
        this->key = std::vector<byte>(AuthorKey::getInstance().get_key().begin(), AuthorKey::getInstance().get_key().end());
    }

    std::vector<byte> AES256Encryption::generate_IV()
    {
        /*IV for AES-CBC - 128 bit*/
        std::vector<byte> IV(16);
        if (RAND_bytes(IV.data(), IV.size()) != 1)
            throw std::runtime_error("Failed to generate random IV");
        return IV;
    }
    AES256Encryption& AES256Encryption::getInstance()
    {
        static AES256Encryption instance;
        return instance;
    }

    void AES256Encryption::set_key(std::array<byte, SHA256_DIGEST_LENGTH> Akey)
    {
        this->key = std::vector<byte>(Akey.begin(), Akey.end());
    }

    void AES256Encryption::set_key(std::vector<byte> Akey)
    {
        this->key = std::move(Akey);
    }

    std::vector<byte> AES256Encryption::encrypt(const std::vector<byte>& data)
    {
        if (data.empty())
            throw std::invalid_argument("data is empty");
        if (this->key.empty())
            throw std::runtime_error("AES256Encryption: key is empty");


        /*Init OpenSSL context*/
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            throw std::runtime_error("AES256Encryption: EVP_CIPHER_CTX_new failed");

        /*IV generation*/
        std::vector<byte> IV = AES256Encryption::generate_IV();
        std::vector<byte> ciphertext(data.size() + IV.size());

        int32_t len = 0;
        int32_t ciphertext_len = 0;

        /*Init encryption*/
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, this->key.data(), IV.data()) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("AES256Encryption: EVP_EncryptInit_ex failed");
        }

        /*Encryption*/
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, data.data(), data.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Encryption failed");
        }
        ciphertext_len = len;

        /*Final*/
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to finalize encryption");
        }
        ciphertext_len += len;

        EVP_CIPHER_CTX_free(ctx);

        /*Realsize vector*/
        ciphertext.resize(ciphertext_len);

        /*ADD IV*/
        std::vector<byte> result;
        result.reserve(ciphertext.size() + IV.size());
        result.insert(result.end(), IV.begin(), IV.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());

        return result;
    }

    std::vector<byte> AES256Encryption::decrypt(const std::vector<byte> &data)
    {
        if (data.empty())
            throw std::invalid_argument("data is empty");
        if (this->key.empty())
            throw std::runtime_error("AES256Encryption: key is empty");

        /*Get IV from data (first 16 bytes)*/
        std::vector<byte> IV(data.begin(), data.begin() + 16);
        std::vector<byte> ciphertext(data.begin() + 16, data.end());

        /*Init OpenSSL context*/
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create EVP context");
        }

        std::vector<byte> plaintext(ciphertext.size());
        int32_t len = 0;
        int32_t plaintext_len = 0;

        /*Init decryption*/
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), IV.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize decryption");
        }

        /*Decryption*/
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Decryption failed");
        }
        plaintext_len = len;

        /*Final*/
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to finalize decryption");
        }
        plaintext_len += len;

        /*Clear context*/
        EVP_CIPHER_CTX_free(ctx);

        /*Actual vector size*/
        plaintext.resize(plaintext_len);

        return plaintext;
    }

} // Yps