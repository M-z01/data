#include "loaders/image/ImageFactory.h"
#include <stdexcept>

std::shared_ptr<Image> ImageFactory::createImage(ImageSourceType type, const std::string& pathOrUri) {
    std::shared_ptr<DataSource> source;

    switch(type) {
        case ImageSourceType::FILE:
            source = std::make_shared<FileDataSource>(pathOrUri);
            break;
        
        case ImageSourceType::ROSBAG:
            source = std::make_shared<ROS2BagDataSource>(pathOrUri);
            break;
        
        default: throw std::invalid_argument("Unsupported image source type");
    }

    return std::make_shared<Image>(source);
}