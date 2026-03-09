#pragma once
#include <memory>
#include <string>
#include "datatype/Image.h"
#include "datasource/FileDataSource.h"
#include "datasource/ROS2BagDataSource.h"


enum class ImageSourceType {
    FILE,
    ROSBAG
};

class ImageFactory {
public:
    static std::shared_ptr<Image> createImage(ImageSourceType type, const std::string& pathOrUri);
};
