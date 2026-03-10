#include "datatype/Image.h"
#include <stdexcept>

void Image::load() {
    std::vector<unsigned char> bytes = source->getRawBytes();
    img = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        throw std::runtime_error("Failed to decode image from data source");
    }

    // check img type
    int channels = img.channels();
    int depth = img.depth();

    /*
        JPG: channels = 3, depth = CV_8U
        PNG: channels = 1 to 4, depth = CV_8U or CV_16U
        EXR: channels = many, depth = CV_32F
    */

    if (channels == 3 && depth == CV_8U) {
        format = ImageFormat::JPG;
    } else if (channels >= 1 && channels <= 4 && (depth == CV_8U || depth == CV_16U)) {
        format = ImageFormat::PNG;
    } else if (depth == CV_32F) {
        format = ImageFormat::EXR;
    } else {
        throw std::runtime_error("Unsupported image format: channels = " + std::to_string(channels) + ", depth = " + std::to_string(depth));
    }
}