#include "loaders/ImageFactory.h"
#include "utils/FormatDetector.h"
#include <stdexcept>

std::shared_ptr<Image> ImageFactory::createImage(ImageSourceType type, const std::string& pathOrUri) {
    auto source = createDataSource(type, pathOrUri);

    const std::string fmtStr = FormatDetector::imageFormat(pathOrUri);
    if (fmtStr.empty())
        throw std::runtime_error("Unrecognised image extension: " + pathOrUri);

    ImageFormat fmt = ImageFormat::UNKNOWN;
    if      (fmtStr == "JPG") fmt = ImageFormat::JPG;
    else if (fmtStr == "PNG") fmt = ImageFormat::PNG;
    else if (fmtStr == "EXR") fmt = ImageFormat::EXR;

    return std::make_shared<Image>(source, fmt);
}