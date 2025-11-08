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
        internal/AuthorKey/AuthorKey.cc
        external/stb_image/stb_image.h
        external/stb_image/stb_image_write.h
        internal/PhotoHnS/PhotoHnS.cc
        internal/PhotoHnS/PhotoHnS.hh
)

find_package(OpenSSL REQUIRED)
# Поиск JPEG: Cross-platform, с fallback.
find_package(JPEG QUIET)  # Не REQUIRED — graceful.
if(NOT JPEG_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(JPEG REQUIRED libjpeg)  # Unix-fallback.
    endif()
endif()

if(NOT JPEG_FOUND)
    message(FATAL_ERROR "libjpeg-turbo/JPEG not found. Install: Linux(apt/dnf/pacman), macOS(brew), Windows(vcpkg).")
endif()

include_directories(internal/
                    internal/HnS
                    internal/Encryption
                    internal/AuthorKey
                    external/
                    external/stb_image
                    ${JPEG_INCLUDE_DIRS}

)

project(${PNAME})
add_executable(${PNAME} ${SRC})

target_link_libraries( ${PNAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto ${JPEG_LIBRARIES})