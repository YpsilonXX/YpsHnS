# YpsHnS - Инструмент Стеганографии

[Перейти к русской версии](#русский)  
[Go to English version](#english)

## Русский

### Оглавление

- [Обзор](#обзор)
- [Функции](#функции)
- [Модули и их назначения](#модули-и-их-назначения)
- [Технологии и методы](#технологии-и-методы)
- [Что сделано](#что-сделано)
- [Что планируется](#что-планируется)
- [Установка](#установка)
- [Вклад](#вклад)
- [Лицензия](#лицензия)
- [Контакты](#контакты)

## Обзор

YpsHnS — это инструмент стеганографии на базе C++, предназначенный для безопасного скрытия данных в мультимедийных файлах, таких как изображения, с будущей поддержкой видео и аудио. Он встраивает зашифрованные данные в контейнеры с использованием замены младших битов (LSB) и дискретного косинусного преобразования (DCT), обеспечивая минимальные визуальные или слуховые искажения. Инструмент уделяет приоритет безопасности через шифрование AES-256 и генерацию уникального ключа на основе машины.

Этот проект идеален для разработчиков, интересующихся цифровой криминалистикой, криптографией и манипуляцией медиа. Он предоставляет чистую, модульную архитектуру для легкого расширения.

## Функции

- **Встраивание и извлечение данных**: Скрытие произвольных бинарных данных (например, файлов, сообщений) внутри изображений и их безпотерьное извлечение.
- **Интеграция шифрования**: Все встраиваемые данные шифруются с использованием AES-256-CBC перед скрытием.
- **Автоматическое определение формата**: Поддержка изображений PNG и JPEG с методами, специфичными для формата.
- **Проверка емкости**: Обеспечение достаточного пространства в контейнере для данных, с предупреждениями о возможных артефактах.
- **RAII для управления ресурсами**: Использование умных указателей и оберток RAII для безопасной работы с библиотеками, такими как libjpeg.
- **Кросс-платформенная совместимость**: Работает на Windows и Linux (с архитектурами x86/x86_64).

## Модули и их назначения

Проект структурирован в модульные компоненты для ясности и удобства поддержки:

- **HnS.hh / HnS.cc** (Интерфейс Скрытия и Поиска):  
  Базовый абстрактный класс, определяющий основной API для встраивания (`embed()`) и извлечения (`extract()`) данных. Включает утилиты для валидации путей.

- **PhotoHnS.hh / PhotoHnS.cc** (Реализация для Фото):  
  Наследует от `HnS` для обработки стеганографии изображений. Поддерживает PNG (через LSB в байтах пикселей) и JPEG (через LSB в коэффициентах DCT). Управляет загрузкой/сохранением с помощью STB и libjpeg-turbo.

- **EmbedData.hh** (Структуры Управления Данными):  
  Определяет `EmbedData` для хранения простых/зашифрованных данных, метаданных (`MetaData`) и перечислений для типов контейнеров, расширений и режимов LSB. Обеспечивает совместимость с POD для безопасных операций с памятью.

- **Encryption.hh / Encryption.cc** (Слой Криптографии):  
  Синглтон-классы для шифрования/дешифрования. Включает базовый `Encryption` (заглушка) и `AES256Encryption` с использованием AES-256-CBC от OpenSSL с генерацией случайного IV.

- **AuthorKey.hh / AuthorKey.cc** (Генерация Ключа):  
  Синглтон для генерации 256-битного уникального ключа машины через хэширование SHA-256 аппаратных идентификаторов (предпочтительно CPUID, с откатом на MAC-адрес или случайный UUID). Используется для инициализации шифрования.

## Технологии и методы

- **Методы Стеганографии**:
    - **Замена LSB (PNG)**: Встраивает 1 или 2 бита на байт в данных пикселей (RGB/RGBA). Использует 1-битный режим для минимальных искажений; переключается на 2-битный при необходимости (с предупреждениями). Порядок битов MSB-first для эффективности.
    - **DCT-LSB (JPEG)**: Встраивает 1 бит на низкочастотный AC-коэффициент (пропуская DC для сохранения качества). Устойчив к незначительной recompрессии.

- **Шифрование**:
    - AES-256-CBC через OpenSSL, с предваряющим случайным IV для каждого сеанса.

- **Библиотеки**:
    - **stb_image / stb_image_write**: Однофайловые библиотеки для загрузки/сохранения PNG (простые, без зависимостей).
    - **libjpeg-turbo**: Высокопроизводительная обработка JPEG с прямым доступом к DCT для эффективного встраивания/извлечения.
    - **OpenSSL**: Для SHA-256 (генерация ключа) и шифрования/дешифрования AES.
    - Стандартный C++: `<filesystem>`, `<memory>`, паттерны RAII для безопасности.

- **Обработка ошибок**: Использует `std::optional` для graceful отказов, с цветным выводом в CLI (например, красный для ошибок, зеленый для успеха).

- **Аспекты безопасности**:
    - Метаданные включают тип контейнера, расширение, имя файла (обрезано до 63 символов) и размер записи для валидации.
    - Проверка альфа-канала в PNG для избежания артефактов (требует полной непрозрачности).
    - Ограничение границ и проверка емкости для предотвращения переполнений.

## Что сделано

- Полная поддержка встраивания/извлечения данных в изображения PNG (режимы LSB 1/2 бита).
- Полная поддержка изображений JPEG (1-битный DCT-LSB, устойчивый к потере качества).
- Интегрированное шифрование AES-256 с генерацией ключа на основе аппаратного обеспечения.
- Автоматические механизмы отката (например, от метаданных пикселей к DCT для гибридных случаев).
- Базовый логирование в CLI с цветными сообщениями для удобства.
- Чистый код с английскими комментариями, объясняющими неочевидную логику (например, манипуляции битами, взаимодействия с библиотеками).

## Что планируется

- **Поддержка видео**: Расширение на MP4/AVI с использованием библиотек вроде FFmpeg для встраивания на основе кадров LSB/DCT.
- **Поддержка аудио**: Скрытие данных в WAV/MP3 через LSB в сэмплах аудио или частотных доменах.
- **Интеграция GUI**: Разработка удобного интерфейса (например, с использованием Qt или ImGui) для выбора файлов, отслеживания прогресса и визуализации скрытых данных.
- **Расширенные функции**: Цепочки нескольких контейнеров, коды коррекции ошибок (например, Reed-Solomon для устойчивости) и пакетная обработка.
- **Тестирование и оптимизация**: Добавление unit-тестов (например, с Google Test), бенчмаркинг метрик искажений (PSNR/SSIM) и поддержка большего количества форматов (например, BMP, GIF).

## Установка

1. **Предварительные требования**:
    - Компилятор C++17 (например, GCC 9+ или MSVC 2019+).
    - libjpeg-turbo (установите через менеджер пакетов: `apt install libjpeg-turbo8-dev` на Ubuntu).
    - OpenSSL (обычно предустановлен; `apt install libssl-dev` если нужно).

2. **Сборка**:
   ```
   mkdir build && cd build
   cmake ..
   make
   ```

3. **Пример использования** (CLI на данный момент):
   ```cpp
   // В main.cpp или тестовом harness:
   Yps::PhotoHnS hns;
   std::vector<byte> data = {/* ваши данные */};
   auto embedded = hns.embed(data, "input.png", "output.png");
   if (embedded) std::cout << "Встроено в: " << *embedded << std::endl;

   auto extracted = hns.extract("output.png");
   if (extracted) std::cout << "Извлечено " << extracted->size() << " байт." << std::endl;
   ```

## Вклад

Вклады приветствуются! Пожалуйста, форкните репозиторий, создайте ветку функции и отправьте pull request. Сосредоточьтесь на чистом коде, английских комментариях и соблюдении существующих паттернов.

## Лицензия

Лицензия MIT — см. [LICENSE](LICENSE) для деталей.

## Контакты

Для вопросов или предложений откройте issue или свяжитесь через GitHub.

*Последнее обновление: 26 декабря 2025 г.*

## English

### Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Modules and Their Purposes](#modules-and-their-purposes)
- [Technologies and Methods](#technologies-and-methods)
- [What's Done](#whats-done)
- [What's Planned](#whats-planned)
- [Installation](#installation)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

## Overview

YpsHnS is a C++-based steganography tool designed to securely hide data within multimedia files such as images, with future support for videos and audio. It embeds encrypted data into containers using least significant bit (LSB) substitution and discrete cosine transform (DCT) techniques, ensuring minimal visual or auditory distortion. The tool prioritizes security through AES-256 encryption and machine-unique key generation.

This project is ideal for developers interested in digital forensics, cryptography, and media manipulation. It provides a clean, modular architecture for easy extension.

## Features

- **Data Embedding and Extraction**: Hide arbitrary binary data (e.g., files, messages) inside images and retrieve it losslessly.
- **Encryption Integration**: All embedded data is encrypted using AES-256-CBC before hiding.
- **Automatic Format Detection**: Supports PNG and JPEG images with format-specific methods.
- **Capacity Checks**: Ensures the container has sufficient space for data, with warnings for potential artifacts.
- **RAII for Resource Management**: Uses smart pointers and RAII wrappers for safe handling of libraries like libjpeg.
- **Cross-Platform Compatibility**: Works on Windows and Linux (with x86/x86_64 architectures).

## Modules and Their Purposes

The project is structured into modular components for clarity and maintainability:

- **HnS.hh / HnS.cc** (Hide and Seek Interface):  
  Base abstract class defining the core API for embedding (`embed()`) and extracting (`extract()`) data. Includes path validation utilities.

- **PhotoHnS.hh / PhotoHnS.cc** (Photo-Specific Implementation):  
  Inherits from `HnS` to handle image steganography. Supports PNG (via LSB in pixel bytes) and JPEG (via LSB in DCT coefficients). Manages loading/saving with STB and libjpeg-turbo.

- **EmbedData.hh** (Data Management Structures):  
  Defines `EmbedData` for holding plain/encrypted data, metadata (`MetaData`), and enums for container types, extensions, and LSB modes. Ensures POD-compatible structures for safe memory operations.

- **Encryption.hh / Encryption.cc** (Cryptography Layer):  
  Singleton classes for encryption/decryption. Includes a base `Encryption` (placeholder) and `AES256Encryption` using OpenSSL's AES-256-CBC with random IV generation.

- **AuthorKey.hh / AuthorKey.cc** (Key Generation):  
  Singleton for generating a 256-bit machine-unique key via SHA-256 hashing of hardware identifiers (CPUID preferred, fallback to MAC address or random UUID). Used for encryption seeding.

## Technologies and Methods

- **Steganography Techniques**:
    - **LSB Substitution (PNG)**: Embeds 1 or 2 bits per byte in pixel data (RGB/RGBA). Uses 1-bit mode for minimal distortion; switches to 2-bit if needed (with warnings). MSB-first bit ordering for efficiency.
    - **DCT-LSB (JPEG)**: Embeds 1 bit per low-frequency AC coefficient (skipping DC to preserve quality). Robust to minor recompression.

- **Encryption**:
    - AES-256-CBC via OpenSSL, with per-session random IV prepended to ciphertext.

- **Libraries**:
    - **stb_image / stb_image_write**: Single-header libraries for PNG loading/saving (simple, no dependencies).
    - **libjpeg-turbo**: High-performance JPEG handling with direct DCT access for efficient embedding/extraction.
    - **OpenSSL**: For SHA-256 (key gen) and AES encryption/decryption.
    - Standard C++: `<filesystem>`, `<memory>`, RAII patterns for safety.

- **Error Handling**: Uses `std::optional` for graceful failures, with colored CLI output (e.g., red for errors, green for success).

- **Security Considerations**:
    - Metadata includes container type, extension, filename (truncated to 63 chars), and write size for validation.
    - Alpha channel checks in PNG to avoid artifacts (requires full opacity).
    - Bounds clamping and capacity verification to prevent overflows.

## What's Done

- Full support for embedding/extracting data in PNG images (1/2-bit LSB modes).
- Full support for JPEG images (1-bit DCT-LSB, robust to quality loss).
- Integrated AES-256 encryption with hardware-based key generation.
- Automatic fallback mechanisms (e.g., pixel meta to DCT for hybrid cases).
- Basic CLI logging with color-coded messages for usability.
- Clean code with English comments explaining non-obvious logic (e.g., bit manipulation, library interactions).

## What's Planned

- **Video Support**: Extend to MP4/AVI using libraries like FFmpeg for frame-based LSB/DCT embedding.
- **Audio Support**: Hide data in WAV/MP3 via LSB in audio samples or frequency domains.
- **GUI Integration**: Develop a user-friendly interface (e.g., using Qt or ImGui) for file selection, progress tracking, and visualization of hidden data.
- **Advanced Features**: Multi-container chaining, error correction codes (e.g., Reed-Solomon for robustness), and batch processing.
- **Testing and Optimization**: Add unit tests (e.g., with Google Test), benchmark distortion metrics (PSNR/SSIM), and support more formats (e.g., BMP, GIF).

## Installation

1. **Prerequisites**:
    - C++17 compiler (e.g., GCC 9+ or MSVC 2019+).
    - libjpeg-turbo (install via package manager: `apt install libjpeg-turbo8-dev` on Ubuntu).
    - OpenSSL (usually pre-installed; `apt install libssl-dev` if needed).

2. **Build**:
   ```
   mkdir build && cd build
   cmake ..
   make
   ```

3. **Usage Example** (CLI for now):
   ```cpp
   // In main.cpp or test harness:
   Yps::PhotoHnS hns;
   std::vector<byte> data = {/* your data */};
   auto embedded = hns.embed(data, "input.png", "output.png");
   if (embedded) std::cout << "Embedded to: " << *embedded << std::endl;

   auto extracted = hns.extract("output.png");
   if (extracted) std::cout << "Extracted " << extracted->size() << " bytes." << std::endl;
   ```

## Contributing

Contributions are welcome! Please fork the repo, create a feature branch, and submit a pull request. Focus on clean code, English comments, and adherence to existing patterns.

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contact

For questions or suggestions, open an issue or reach out via GitHub.

*Last Updated: December 26, 2025*