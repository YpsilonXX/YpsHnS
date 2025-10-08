#include "AuthorKey.hh"
namespace Yps
{

    AuthorKey::AuthorKey()
    {
        /*Check CPUID*/
        std::optional<std::string> cpuid = this->get_cpu_id();
        if (cpuid)
        {
            this->id_type = IDType::CPUID;
            this->generate_key(*cpuid);
            return;
        }

        /*Fallback(MAC)*/
        std::optional<std::string> mac = get_mac_address();
        if (mac)
        {
            this->id_type = IDType::MAC;
            this->generate_key(*mac);
            return;
        }

        /*Fallback(UUID)*/
        this->id_type = IDType::UUID;
        this->generate_key(this->generate_uuid());
    }



    std::optional<std::string> AuthorKey::get_cpu_id() const
    {
    #if defined(_WIN32) || defined(__x86_64__) || defined(__i386__)
            // Get CPUID (eax=1 for base info).
            int cpuInfo[4] = {0};
    #ifdef _WIN32
            __cpuid(cpuInfo, 1);
    #else
            __get_cpuid(1, (unsigned int*)&cpuInfo[0], (unsigned int*)&cpuInfo[1],
                        (unsigned int*)&cpuInfo[2], (unsigned int*)&cpuInfo[3]);
    #endif

            // Check data: stepping, model, family.
            int stepping = cpuInfo[0] & 0xF;
            int model = (cpuInfo[0] >> 4) & 0xF;
            int family = (cpuInfo[0] >> 8) & 0xF;
            int extended_model = (cpuInfo[0] >> 16) & 0xF;
            int extended_family = (cpuInfo[0] >> 20) & 0xFF;

            // Additionally: Vendor ID (eax=0).
            int vendorInfo[4] = {0};
    #ifdef _WIN32
            __cpuid(vendorInfo, 0);
    #else
            __get_cpuid(0, (unsigned int*)&vendorInfo[0], (unsigned int*)&vendorInfo[1],
                        (unsigned int*)&vendorInfo[2], (unsigned int*)&vendorInfo[3]);
    #endif
            char vendor[13] = {0};
            memcpy(vendor, &vendorInfo[1], 4);  // ebx
            memcpy(vendor + 4, &vendorInfo[3], 4);  // edx
            memcpy(vendor + 8, &vendorInfo[2], 4);  // ecx

            // Make string.
            std::ostringstream oss;
            oss << vendor << ":" << std::hex << family << extended_family << model << extended_model << stepping;
            return oss.str();  // Например: "GenuineIntel:0f00a1"
    #else
            // CPUID Forbidden or NotFound.
            return std::nullopt;
    #endif
    }


    std::optional<std::string> AuthorKey::get_mac_address() const
    {
    #ifdef _WIN32
                ULONG bufferSize = 0;
                GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &bufferSize);
                std::vector<BYTE> buffer(bufferSize);
                PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
                if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &bufferSize) != ERROR_SUCCESS) {
                        return std::nullopt;
                }
                while (adapters) {
                        if (adapters->PhysicalAddressLength == 6) {
                                std::ostringstream oss;
                                for (int i = 0; i < 6; ++i) {
                                        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(adapters->PhysicalAddress[i]);
                                        if (i < 5) oss << ':';
                                }
                                return oss.str();
                        }
                        adapters = adapters->Next;
                }
                return std::nullopt;
    #else
                struct ifaddrs* ifaddr;
                if (getifaddrs(&ifaddr) == -1) {
                        return std::nullopt;
                }
                std::optional<std::string> result;
                for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_PACKET) {
                                struct sockaddr_ll* s = reinterpret_cast<struct sockaddr_ll*>(ifa->ifa_addr);
                                if (s->sll_halen == 6) {
                                        std::ostringstream oss;
                                        for (int i = 0; i < 6; ++i) {
                                                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(s->sll_addr[i]);
                                                if (i < 5) oss << ':';
                                        }
                                        result = oss.str();
                                        break;
                                }
                        }
                }
                freeifaddrs(ifaddr);
                return result;
    #endif
}


    std::string AuthorKey::generate_uuid() const
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        std::string uuid(16, 0);
        for (auto& byte : uuid) {
                byte = dis(gen);
        }
        return uuid;
    }


    void AuthorKey::generate_key(const std::string &seed)
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(seed.c_str()), seed.size(), hash);
        std::copy(hash, hash + SHA256_DIGEST_LENGTH, key_.begin());
    }

    std::string AuthorKey::get_author_id() const
    {
        std::ostringstream oss;
        for (auto byte : key_) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        return oss.str();
    }

    std::string AuthorKey::get_id_type() const
    {
        switch (this->id_type)
        {
            case IDType::CPUID:
                return "CPUID";
            case IDType::MAC:
                return "MAC";
            case IDType::UUID:
                return "UUID";
            default:
                return "Error! No type";
        }
    }


}