#include "loaders/image/ImageFactory.h"
#include "utils/DataConverter.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>
#include <set>

namespace fs = std::filesystem;

static const std::set<std::string> IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".exr"};

// Return lowercase version of a string
static std::string toLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(c));
    return s;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <input_dir> <output_dir> <target_format>\n"
                  << "  target_format: JPG, PNG, EXR\n";
        return 1;
    }

    const fs::path inputDir  = argv[1];
    const fs::path outputDir = argv[2];
    const std::string targetFmt = argv[3];

    if (!fs::is_directory(inputDir)) {
        std::cerr << "Error: input directory does not exist: " << inputDir << "\n";
        return 1;
    }

    // Derive the output extension from the target format
    std::string fmtLower = toLower(targetFmt);
    std::string outExt = (fmtLower == "jpg" || fmtLower == "jpeg") ? ".jpg"
                       : (fmtLower == "png")                        ? ".png"
                       : (fmtLower == "exr")                        ? ".exr"
                       : "";
    if (outExt.empty()) {
        std::cerr << "Error: unsupported target format '" << targetFmt
                  << "'. Supported: JPG, PNG, EXR\n";
        return 1;
    }

    fs::create_directories(outputDir);

    int converted = 0, skipped = 0, failed = 0;

    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;

        const fs::path& srcPath = entry.path();
        std::string ext = toLower(srcPath.extension().string());

        if (IMAGE_EXTS.find(ext) == IMAGE_EXTS.end()) {
            // Not a recognised image — skip silently
            continue;
        }

        // Build output path: same stem, new extension
        fs::path dstPath = outputDir / (srcPath.stem().string() + outExt);

        try {
            auto img = ImageFactory::createImage(ImageSourceType::FILE, srcPath.string());
            img->load();

            auto converted_img = DataConverter::convertImageFormat(img, targetFmt);

            if (!cv::imwrite(dstPath.string(), converted_img->getImage())) {
                throw std::runtime_error("cv::imwrite failed");
            }

            std::cout << "[OK]   " << srcPath.filename().string()
                      << "  ->  " << dstPath.filename().string() << "\n";
            ++converted;

        } catch (const std::exception& e) {
            std::cerr << "[FAIL] " << srcPath.filename().string()
                      << ": " << e.what() << "\n";
            ++failed;
        }
    }

    std::cout << "\nDone: " << converted << " converted, "
              << failed << " failed, " << skipped << " skipped.\n";

    return (failed > 0) ? 1 : 0;
}
