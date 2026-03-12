#include "loaders/ImageFactory.h"
#include "utils/FormatDetector.h"
#include <stdexcept>

std::shared_ptr<Image> ImageFactory::createImage(ImageSourceType type, const std::string& pathOrUri) {
    auto source = createDataSource(type, pathOrUri);

    const std::string fmtStr = FormatDetector::imageFormat(pathOrUri);
    if (fmtStr.empty())
        throw std::runtime_error("Unrecognised image extension: " + pathOrUri);

    return std::make_shared<Image>(source, imageFormatFromString(fmtStr));
}