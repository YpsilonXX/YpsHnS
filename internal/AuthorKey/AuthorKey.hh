#ifndef YPSHNS_AUTHORKEY_HH
#define YPSHNS_AUTHORKEY_HH
#include <memory>
#include <cstring>
#include <optional>
#include <iomanip>
#include <sstream>
#include <array>
#include <random>
#include <defines.hh>
#include <openssl/sha.h>
#include <iostream>

#ifdef _WIN32
#include <intrin.h>  // __cpuid
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <cpuid.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <unistd.h>
#endif

namespace Yps
{
    /**
     * Singleton class to gen machine-unique 256bit key.
     * If gen type UUID - key will be unique for every generation
     */
    class AuthorKey
    {
    private:
        AuthorKey();

        static std::unique_ptr<AuthorKey> instance;
        /**
         * Key for cryptography and identification
         */
        std::array<byte, SHA256_DIGEST_LENGTH> key_;

        enum class IDType {CPUID, MAC, UUID};
        IDType id_type;

        /**
         * Get unique id for identification and cryptography
         * @return string with id
         * @note working with x86/x86_64
         */
        [[nodiscard]]std::optional<std::string> get_cpu_id() const;

        /**
         * Get Mac Address for identification and cryptography
         * @return string with address
         * @note Fallback for : get_cpu_id()
         */
        [[nodiscard]]std::optional<std::string> get_mac_address() const;

        /**
         * Generate id for identification and cryptography
         * @return string with uuid
         * @note Fallback for : get_cpu_id()
         */
        [[nodiscard]]std::string generate_uuid() const;

        /**
         * Generate AuthorKey
         * @param seed Seed for SHA256
         * @note Result in AuthorKey::key_
         */
        void generate_key(const std::string& seed);

    public:

        /**
        * Forbidden copy and "=" constructor
        */
        AuthorKey(const AuthorKey&) = delete;
        AuthorKey& operator=(const AuthorKey&) = delete;

        static AuthorKey& getInstance()
        {
            static AuthorKey instance;
            return instance;
        }

        /**
         *
         * @return Key for cryptography and identification
         */
        std::array<byte, SHA256_DIGEST_LENGTH> get_key()
        { return this->key_; }

        /**
         *
         * @return string with id
         */
        std::string get_author_id() const;

        /**
         * Get what's type of ID was generated (which seed source)
         * @return string with id_type
         */
        std::string get_id_type() const;

    };
} // Yps

#endif //YPSHNS_AUTHORKEY_HH