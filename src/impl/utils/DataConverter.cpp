#include "utils/DataConverter.h"
#include "datasource/MemoryDataSource.h"
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <algorithm>
#include <cctype>

// Map a user-supplied format string (case-insensitive) to an OpenCV file extension.
static std::string formatToExtension(const std::string& fmt) {
    std::string upper = fmt;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "JPG" || upper == "JPEG") return ".jpg";
    if (upper == "PNG")                    return ".png";
    if (upper == "EXR")                    return ".exr";

    throw std::invalid_argument("Unsupported target format: " + fmt +
                                ". Supported: JPG, JPEG, PNG, EXR");
}

std::shared_ptr<Image> DataConverter::convertImageFormat(
    const std::shared_ptr<Image>& img,
    const std::string& targetFormat
) {
    // Ensure the source image is decoded
    if (img->getImage().empty()) {
        img->load();
    }

    const std::string ext = formatToExtension(targetFormat);

    // Encode the cv::Mat into the requested format
    std::vector<unsigned char> buf;
    if (!cv::imencode(ext, img->getImage(), buf)) {
        throw std::runtime_error("cv::imencode failed for format: " + targetFormat);
    }

    // Wrap the encoded bytes in a MemoryDataSource and build a new Image
    auto memSrc   = std::make_shared<MemoryDataSource>(std::move(buf));
    auto converted = std::make_shared<Image>(memSrc);
    converted->load(); // decode & detect format

    return converted;
}

std::shared_ptr<Video> DataConverter::imagesToVideo(
    const std::vector<std::shared_ptr<Image>>& images,
    double fps
) {
    // TODO: implement
    throw std::runtime_error("imagesToVideo not yet implemented");
}

std::vector<std::shared_ptr<Image>> DataConverter::videoToImages(
    const std::shared_ptr<Video>& video
) {
    // TODO: implement
    throw std::runtime_error("videoToImages not yet implemented");
}