#ifndef YPSHNS_AUTHORKEY_HH
#define YPSHNS_AUTHORKEY_HH
#include <memory>


namespace Yps
{
    class AuthorKey
    {
    private:
        AuthorKey();

        static std::unique_ptr<AuthorKey> instance;

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
    };
} // Yps

#endif //YPSHNS_AUTHORKEY_HH