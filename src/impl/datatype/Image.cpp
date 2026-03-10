#include "datatype/Image.h"
#include "utils/OpenCVBridge.h"
#include <stdexcept>

void Image::load() {
    std::vector<unsigned char> bytes = source->getRawBytes();
    cv::Mat mat = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
    if (mat.empty()) {
        throw std::runtime_error("Failed to decode image from data source");
    }

    // check img type
    int channels = mat.channels();
    int depth = mat.depth();

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

    img = OpenCVBridge::matToBuffer(mat);
}

void Image::saveToFile(const std::string& path) const {
    if (img.empty()) {
        throw std::runtime_error("Cannot save: image not loaded");
    }
    cv::Mat mat = OpenCVBridge::bufferToMat(img);
    if (!cv::imwrite(path, mat)) {
        throw std::runtime_error("cv::imwrite failed for path: " + path);
    }
}