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

namespace Yps
{
    /**
     * Sample Encryption class
     */
    class Encryption
    {
    private:
        Encryption();

        /**
         * Forbidden copy and "=" constructor
         */
        Encryption(const Encryption&) = delete;
        Encryption& operator=(const Encryption&) = delete;

    public:
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

} // Yps

#endif //YPSHNS_ENCRYPTION_HH