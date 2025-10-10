/**
 *
 *  For some time there will be no encryption
 *          Need to add in future
 */
#ifndef YPSHNS_ENCRYPTION_HH
#define YPSHNS_ENCRYPTION_HH
#include <memory>
#include <vector>
#include <defines.hh>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "AuthorKey.hh"

namespace Yps
{
    /**
     * Sample Encryption class
     */
    class Encryption
    {
    private:
        Encryption(){};


    public:

        /**
        * Forbidden copy and "=" constructor
        */
        Encryption(const Encryption&) = delete;
        Encryption& operator=(const Encryption&) = delete;

        static Encryption& getInstance();

        /**
         * Encrypt data
         * @param data Vector of bytes
         * @return Vector of encrypted bytes
         */
        std::vector<byte> encrypt(const std::vector<byte>& data);

        /**
         * Decrypt data
         * @param data Vector of encrypted bytes
         * @return Vector of decrypted bytes
         */
        std::vector<byte> decrypt(const std::vector<byte>& data);
    };

    class AES256Encryption
    {
    private:
        AES256Encryption();

        /**
         * Secret key. Pre-defined in Constructor by AuthorKey. Can be changed.
         */
        std::vector<byte> key;

        /**
         * Generate IV for every encryption
         * @return IV init vector
         */
        static std::vector<byte> generate_IV();
    public:
        /**
         * Forbidden copy and "=" constructor
         */
        AES256Encryption(const AES256Encryption&) = delete;
        AES256Encryption& operator=(const AES256Encryption&) = delete;

        static AES256Encryption& getInstance();

        /**
         * Set new secrey key
         * @param Akey array with key
         */
        void set_key(std::array<byte, SHA256_DIGEST_LENGTH> Akey);

        /**
         * Set new secrey key
         * @param Akey array with key
         */
        void set_key(std::vector<byte> Akey);

        /**
         * Encryption with AES-256-CBC
         * @param data Vector of bytes to encrypt
         * @return Encrypt data
         */
        std::vector<byte> encrypt(const std::vector<byte>& data);

        /**
         * Decryption with AES-256-CBC
         * @param data Vector of encrypted bytes (IV + ciphertext)
         * @return Decrypted data
         */
        std::vector<byte> decrypt(const std::vector<byte>& data);
    };


} // Yps

#endif //YPSHNS_ENCRYPTION_HH