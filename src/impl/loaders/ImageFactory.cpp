#include "loaders/ImageFactory.h"
#include <stdexcept>

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

    return std::make_shared<Image>(source);
}