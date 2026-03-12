#include "loaders/ImageFactory.h"
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

static ImageFormat resolveImageFormat(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    if (ext == ".jpg" || ext == ".jpeg") return ImageFormat::JPG;
    if (ext == ".png")                   return ImageFormat::PNG;
    if (ext == ".exr")                   return ImageFormat::EXR;
    return ImageFormat::UNKNOWN;
}

std::shared_ptr<Image> ImageFactory::createImage(ImageSourceType type, const std::string& pathOrUri) {
    std::shared_ptr<DataSource> source;

    switch(type) {
        case ImageSourceType::FILE:
            source = std::make_shared<FileDataSource>(pathOrUri);
            break;
        
        case ImageSourceType::STREAM:
            // TODO: Implement stream data source
            throw std::runtime_error("Stream data source not implemented yet");
            break;
        
        default: throw std::invalid_argument("Unsupported image source type");
    }

    ImageFormat fmt = resolveImageFormat(pathOrUri);
    if (fmt == ImageFormat::UNKNOWN)
        throw std::runtime_error("Unrecognised image extension: " + pathOrUri);

    return std::make_shared<Image>(source, fmt);
}