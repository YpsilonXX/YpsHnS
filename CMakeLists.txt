cmake_minimum_required(VERSION 3.12)

set(PNAME YpsHnS)
project(${PNAME} CXX)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Set policy to allow AUTOMOC/AUTOUIC on .hh files
if(POLICY CMP0100)
    cmake_policy(SET CMP0100 NEW)
endif()

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
        internal/GUI/GUI.cc
        internal/GUI/GUI.hh
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

# Find Qt6 (fallback to Qt5 if not found).
find_package(Qt6 COMPONENTS Core Gui Widgets Concurrent)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS Core Gui Widgets Concurrent REQUIRED)
endif()

# Enable automatic handling for Qt meta-object compiler (moc), resource compiler (rcc), and UI compiler (uic).
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Conditional include directories and link libraries
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_INCLUDE_DIRS ${Qt6_INCLUDE_DIRS})
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
else()
    set(QT_VERSION_MAJOR 5)
    set(QT_INCLUDE_DIRS ${Qt5_INCLUDE_DIRS})
    set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent)
endif()

include_directories(internal/
        internal/HnS
        internal/Encryption
        internal/AuthorKey
        internal/PhotoHnS  # Added for PhotoHnS.hh
        internal/GUI      # Added for GUI.hh (if needed)
        external/
        external/stb_image
        ${JPEG_INCLUDE_DIRS}
        ${QT_INCLUDE_DIRS}
)

add_executable(${PNAME} ${SRC})

target_link_libraries( ${PNAME} PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ${JPEG_LIBRARIES}
        ${QT_LIBRARIES}
)