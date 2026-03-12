#include "loaders/PointcloudFactory.h"
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

static PointcloudFormat resolvePointcloudFormat(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    if (ext == ".pcd") return PointcloudFormat::PCD;
    if (ext == ".ply") return PointcloudFormat::PLY;
    return PointcloudFormat::UNKNOWN;
}

std::shared_ptr<Pointcloud> PointcloudFactory::createPointcloud(PointcloudSourceType type, const std::string& pathOrUri) {
    std::shared_ptr<DataSource> source;

    switch (type) {
        case PointcloudSourceType::FILE:
            source = std::make_shared<FileDataSource>(pathOrUri);
            break;

        case PointcloudSourceType::STREAM:
            // TODO: Implement stream data source
            throw std::runtime_error("Stream data source not implemented yet");
            break;

        default: throw std::invalid_argument("Unsupported pointcloud source type");
    }

    PointcloudFormat fmt = resolvePointcloudFormat(pathOrUri);
    if (fmt == PointcloudFormat::UNKNOWN)
        throw std::runtime_error("Unrecognised pointcloud extension: " + pathOrUri);

    return std::make_shared<Pointcloud>(source, fmt);
}
