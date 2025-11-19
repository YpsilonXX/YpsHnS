cmake_minimum_required(VERSION 3.16)
project(YpsHnS VERSION 1.0 LANGUAGES CXX)

cmake_policy(SET CMP0100 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug по умолчанию
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ------------------- Источники -------------------
set(SRC
        main.cc

        internal/HnS/HnS.cc
        internal/HnS/HnS.hh
        external/defines.hh
        internal/HnS/EmbedData.hh

        internal/Encryption/Encryption.cc
        internal/Encryption/Encryption.hh

        internal/AuthorKey/AuthorKey.cc
        internal/AuthorKey/AuthorKey.hh

        external/stb_image/stb_image.h
        external/stb_image/stb_image_write.h

        internal/PhotoHnS/PhotoHnS.cc
        internal/PhotoHnS/PhotoHnS.hh

        internal/Gui/MainWindow.cc
        internal/Gui/MainWindow.hh
        internal/Gui/KeyDialog.cc
        internal/Gui/KeyDialog.hh
)

# ------------------- Qt6 -------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
qt6_standard_project_setup()          # ← автоматически настраивает moc/rcc/uic

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ------------------- OpenSSL -------------------
find_package(OpenSSL REQUIRED)

# ------------------- libjpeg (turbo) -------------------
find_package(JPEG QUIET)
if(NOT JPEG_FOUND)
    message(STATUS "find_package(JPEG) не нашёл → пробуем pkg-config")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(JPEG REQUIRED libjpeg)
    endif()
endif()

if(NOT JPEG_FOUND)
    message(FATAL_ERROR
            "libjpeg не найден!\n"
            "Linux:   sudo apt install libjpeg-turbo8-dev   (Ubuntu/Debian)\n"
            "         sudo dnf install libjpeg-turbo-devel  (Fedora)\n"
            "macOS:   brew install jpeg-turbo\n"
            "Windows: vcpkg install libjpeg-turbo\n"
    )
endif()

# ------------------- Исполняемый файл -------------------
add_executable(${PROJECT_NAME} ${SRC})

# ------------------- Include directories (современный способ) -------------------
target_include_directories(${PROJECT_NAME} PRIVATE
        internal
        internal/HnS
        internal/Encryption
        internal/AuthorKey
        internal/PhotoHnS
        internal/Gui
        external
        external/stb_image
        ${JPEG_INCLUDE_DIRS}
)

# ------------------- Link libraries -------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        OpenSSL::SSL
        OpenSSL::Crypto
        ${JPEG_LIBRARIES}
)

# ------------------- Финальные приятные мелочи -------------------
# Красивый вывод в консоли
message(STATUS "=== YpsHnS успешно настроен ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Qt6:        ${Qt6_DIR}")
message(STATUS "OpenSSL:    ${OPENSSL_VERSION}")
message(STATUS "JPEG:       ${JPEG_INCLUDE_DIRS}")