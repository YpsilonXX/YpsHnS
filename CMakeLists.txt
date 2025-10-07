cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(PNAME YpsHnS)

set(SRC main.cc
        internal/HnS/HnS.cc
        internal/HnS/HnS.hh
        external/defines.hh
        internal/HnS/EmbedData.hh
        internal/Encryption/Encryption.cc
        internal/Encryption/Encryption.hh
        internal/AuthorKey/AuthorKey.hh

)

find_package(OpenSSL REQUIRED)

include_directories(internal/
                    internal/HnS
                    internal/Encryption
                    internal/AuthorKey
                    external/

)

project(${PNAME})
add_executable(${PNAME} ${SRC})

target_link_libraries( ${PNAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)