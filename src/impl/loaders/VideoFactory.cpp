#include "loaders/VideoFactory.h"
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

static VideoFormat resolveVideoFormat(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    if (ext == ".mp4" || ext == ".mov" || ext == ".m4v") return VideoFormat::MP4;
    if (ext == ".avi")                                    return VideoFormat::AVI;
    if (ext == ".mkv" || ext == ".webm")                  return VideoFormat::MKV;
    return VideoFormat::UNKNOWN;
}

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

    VideoFormat fmt = resolveVideoFormat(pathOrUri);
    if (fmt == VideoFormat::UNKNOWN)
        throw std::runtime_error("Unrecognised video extension: " + pathOrUri);

    return std::make_shared<Video>(source, fmt);
}
