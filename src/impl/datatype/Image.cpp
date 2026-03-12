#include "datatype/Image.h"
#include "utils/OpenCVBridge.h"
#include <stdexcept>

void Image::load() {
    if (format == ImageFormat::UNKNOWN)
        throw std::runtime_error("Image format not set before load()");

    const std::vector<unsigned char>& bytes = source->getRawBytes();
    cv::Mat mat = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
    if (mat.empty()) {
        throw std::runtime_error("Failed to decode image from data source");
    }

    img = OpenCVBridge::matToBuffer(mat);
    loaded_ = true;
}

void Image::saveToFile(const std::string& path, ImageFormat fmt, PixelType pixelType) const {
    requireLoaded("Image");

    cv::Mat mat = OpenCVBridge::bufferToMat(img);

    // Convert to the requested bit depth
    switch (pixelType) {
        case PixelType::UINT8:
            if (mat.depth() != CV_8U)
                mat.convertTo(mat, CV_8U,
                    mat.depth() == CV_16U ? 1.0 / 256.0 : 255.0);
            break;
        case PixelType::UINT16:
            if (mat.depth() != CV_16U)
                mat.convertTo(mat, CV_16U,
                    mat.depth() == CV_8U ? 256.0 : 65535.0);
            break;
        case PixelType::FLOAT32:
            if (mat.depth() != CV_32F)
                mat.convertTo(mat, CV_32F,
                    mat.depth() == CV_8U ? 1.0 / 255.0 : 1.0 / 65535.0);
            break;
    }

    if (!cv::imwrite(path, mat))
        throw std::runtime_error("cv::imwrite failed for path: " + path);
}

void Image::saveToFile(const std::string& path) const {
    saveToFile(path, format, img.pixelType);
}