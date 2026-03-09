#include "loaders/image/ImageFactory.h"
#include <iostream>

int main() {
    try {
        auto img = ImageFactory::createImage(ImageSourceType::FILE, "/home/kewei/repo/claw2cam/test_claw_pose/color/000380.png");
        img->load();

        std::cout << "Loaded image: " 
                  << img->getImage().cols << "x" 
                  << img->getImage().rows << std::endl;

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
