#include "loaders/video/VideoFactory.h"
#include <stdexcept>

std::shared_ptr<Video> VideoFactory::createVideo(VideoSourceType type, const std::string& pathOrUri) {
    std::shared_ptr<DataSource> source;

    switch (type) {
        case VideoSourceType::FILE:
            source = std::make_shared<FileDataSource>(pathOrUri);
            break;

        case VideoSourceType::STREAM:
            // TODO: Implement stream data source
            throw std::runtime_error("Stream data source not implemented yet");
            break;

        default: throw std::invalid_argument("Unsupported video source type");
    }

    return std::make_shared<Video>(source);
}
