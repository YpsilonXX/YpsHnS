#include <iostream>
#include <AuthorKey.hh>
#include <cstdlib>

#include "EmbedData.hh"
#include "Encryption.hh"
#include "PhotoHnS/PhotoHnS.hh"

// Helper function to perform embedding, extraction, and verification for a given format.
// This encapsulates the common logic for PNG and JPEG tests, promoting code reuse and clarity.
// Parameters:
// - ph: Reference to PhotoHnS instance (reused across tests for efficiency).
// - data: The original data to embed and verify against.
// - input_path: Path to the input carrier file (e.g., "p_in.png").
// - output_path: Path to the output embedded file (e.g., "p_out.png").
// Returns: true if the test passes (embed, extract, and match successful), false otherwise.
bool run_test(Yps::PhotoHnS& ph, const std::vector<byte>& data,
              const std::string& input_path, const std::string& output_path,
              const std::string& format) {
    // Attempt to embed data into the input file.
    std::optional<std::string> embed_result = ph.embed(data, input_path, output_path);
    if (!embed_result.has_value()) {
        std::cerr << format << " embedding failed: Check '" << input_path
                  << "' existence, format, and capacity." << std::endl;
        return false;
    }

    // Extract data from the output file.
    std::optional<std::vector<byte>> extracted = ph.extract(output_path);
    if (!extracted.has_value()) {
        std::cerr << format << " extraction failed: Verify '" << output_path
                  << "' integrity." << std::endl;
        return false;
    }

    // Verify sizes match.
    if (data.size() != extracted.value().size()) {
        std::cout << format << " size mismatch: expected " << data.size()
                  << ", got " << extracted.value().size() << std::endl;
        return false;
    }

    // Byte-by-byte comparison.
    bool mismatch = false;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] != extracted.value()[i]) {
            std::cout << format << " data mismatch at byte " << i
                      << ": expected " << static_cast<int>(data[i])
                      << ", got " << static_cast<int>(extracted.value()[i]) << std::endl;
            mismatch = true;
            break;  // Early exit on first mismatch to avoid unnecessary checks.
        }
    }

    if (mismatch) {
        std::cout << format << " test failed: Data mismatch detected." << std::endl;
        return false;
    }

    // Print extracted data as string for visual confirmation.
    std::string extracted_str(reinterpret_cast<const char*>(extracted.value().data()),
                              extracted.value().size());
    std::cout << format << " extracted: " << extracted_str << std::endl;

    std::cout << format << " test passed: Data embedded and extracted successfully!" << std::endl;
    return true;
}

int main(void) {
    // Print author ID as a hexadecimal string for verification.
    std::cout << Yps::AuthorKey::getInstance().get_author_id() << std::endl << std::endl;

    // Print the key bytes as decimal values (without separators) for byte-level inspection.
    for (uint32_t i = 0; i < Yps::AuthorKey::getInstance().get_key().size(); ++i) {
        std::cout << static_cast<uint16_t>(Yps::AuthorKey::getInstance().get_key()[i]);
    }
    std::cout << std::endl << std::endl;

    // Print the type of seed used for key generation (e.g., CPUID, MAC, UUID).
    std::cout << "Type of seed: " << Yps::AuthorKey::getInstance().get_id_type() << std::endl;

    std::cout << "-------------------" << std::endl;

    // Prepare test data: A simple string converted to byte vector.
    std::string line = "Это зашифрованный текст ";
    std::cout << line << std::endl;
    std::vector<byte> data(line.c_str(), line.c_str() + line.size());

    // Create a single PhotoHnS instance (reused for both tests to minimize overhead).
    Yps::PhotoHnS ph;

    // Run PNG test.
    // Assumes 'p_in.png' is a valid PNG carrier image with sufficient capacity.
    if (!run_test(ph, data, "p_in.png", "p_out.png", "PNG")) {
        return 1;  // Exit early if PNG test fails.
    }

    std::cout << "-------------------" << std::endl;

    Yps::PhotoHnS ph2;
    // Run JPEG test.
    // Assumes 'j_in.jpg' is a valid JPEG carrier image with sufficient capacity.
    if (!run_test(ph2, data, "j_in.jpg", "j_out.jpg", "JPEG")) {
        return 1;  // Exit early if JPEG test fails.
    }

    return 0;  // All tests passed.
}