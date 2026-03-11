#include "loaders/ImageFactory.h"
#include "utils/DataInfo.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_dir1> <input_dir2>\n";
        return 1;
    }

    const fs::path inputDir = argv[1];
    const fs::path inputDir2 = argv[2];

    auto img = ImageFactory::createImage(ImageSourceType::FILE, inputDir.string());
    img->load();

    DataInfo::printImageInfo(img, "mask");
    DataInfo::displayImage(img, "mask");

    auto img2 = ImageFactory::createImage(ImageSourceType::FILE, inputDir2.string());
    img2->load();
    
    DataInfo::printImageInfo(img2, "depth");
    DataInfo::displayImage(img2, "depth");
    return 0;
}