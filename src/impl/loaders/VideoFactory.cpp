#include "loaders/VideoFactory.h"
#include "utils/FormatDetector.h"
#include <stdexcept>

std::shared_ptr<Video> VideoFactory::createVideo(VideoSourceType type, const std::string& pathOrUri) {
    auto source = createDataSource(type, pathOrUri);

    const std::string fmtStr = FormatDetector::videoFormat(pathOrUri);
    if (fmtStr.empty())
        throw std::runtime_error("Unrecognised video extension: " + pathOrUri);

    VideoFormat fmt = VideoFormat::UNKNOWN;
    if      (fmtStr == "MP4") fmt = VideoFormat::MP4;
    else if (fmtStr == "AVI") fmt = VideoFormat::AVI;
    else if (fmtStr == "MKV") fmt = VideoFormat::MKV;

    return std::make_shared<Video>(source, fmt);
}
