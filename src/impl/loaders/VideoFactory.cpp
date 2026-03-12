#include "loaders/VideoFactory.h"
#include "utils/FormatDetector.h"
#include <stdexcept>

std::shared_ptr<Video> VideoFactory::createVideo(VideoSourceType type, const std::string& pathOrUri) {
    auto source = createDataSource(type, pathOrUri);

    const std::string fmtStr = FormatDetector::videoFormat(pathOrUri);
    if (fmtStr.empty())
        throw std::runtime_error("Unrecognised video extension: " + pathOrUri);

    return std::make_shared<Video>(source, videoFormatFromString(fmtStr));
}
