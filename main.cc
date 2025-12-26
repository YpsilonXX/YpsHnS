// main.cc
#include <QtWidgets/QApplication>
#include "internal/GUI/GUI.hh"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.setWindowTitle("StegoApp - Hide and Seek");
    window.resize(1200, 800);
    window.show();
    return app.exec();
}

/*
int main(void)
{
    // Print author info for verification (same as PNG test).
    std::cout << Yps::AuthorKey::getInstance().get_author_id() << std::endl << std::endl;
    for (uint32_t i = 0; i < Yps::AuthorKey::getInstance().get_key().size(); ++i)
    {
        std::cout << static_cast<uint16_t>(Yps::AuthorKey::getInstance().get_key()[i]);
    }
    std::cout << std::endl << std::endl;

    std::cout << "Type of seed: " << Yps::AuthorKey::getInstance().get_id_type() << std::endl;

    std::cout << "-------------------" << std::endl;

    // Prepare data to embed (same test string as in PNG example).
    std::string line = "Это зашифрованный текст ";
    std::cout << line << std::endl;

    std::vector<byte> data(line.c_str(), line.c_str() + line.size());

    // Create PhotoHnS instance for JPEG testing.
    // Note: Ensure 'j_in.jpg' exists as a valid JPEG carrier image with sufficient capacity.
    //       The embed will fail gracefully if capacity is insufficient or file invalid.
    Yps::PhotoHnS ph;
    std::optional<std::string> embed_result = ph.embed(data, "j_in.jpg", "j_out.jpg");
    if (!embed_result.has_value()) {
        std::cerr << "Embedding failed: Check 'j_in.jpg' existence, format, and capacity." << std::endl;
        return 1;  // Exit on embed failure for clear testing.
    }

    // Extract from output JPEG.
    // Note: Extraction uses fallback DCT if pixel-based metadata fails (robust to JPEG specifics).
    std::optional<std::vector<byte>> d_out = ph.extract("j_out.jpg");
    if (!d_out.has_value()) {
        std::cerr << "Extraction failed: Verify 'j_out.jpg' integrity." << std::endl;
        return 1;  // Exit on extract failure.
    }

    // Verify extracted data matches original (byte-by-byte).
    if (data.size() != d_out.value().size()) {
        std::cout << "Size mismatch: expected " << data.size() << ", got " << d_out.value().size() << std::endl;
        return 1;
    }
    bool mismatch = false;
    for (size_t i = 0; i < data.size(); ++i) {  // Use size_t for loop (cleaner than int).
        if (data[i] != d_out.value()[i]) {
            std::cout << "Data mismatch at byte " << i << ": expected " << static_cast<int>(data[i])
                      << ", got " << static_cast<int>(d_out.value()[i]) << std::endl;
            mismatch = true;
            break;  // Stop on first mismatch for efficiency.
        }
    }
    if (!mismatch) {
        std::cout << "JPEG test passed: Data embedded and extracted successfully!" << std::endl;
    } else {
        std::cout << "JPEG test failed: Data mismatch detected." << std::endl;
        return 1;
    }

    // Optional: Print extracted data as string for visual confirmation.
    std::string extracted_str(reinterpret_cast<const char*>(d_out.value().data()), d_out.value().size());
    std::cout << "Extracted: " << extracted_str << std::endl;

    return 0;  // Success.
}*/