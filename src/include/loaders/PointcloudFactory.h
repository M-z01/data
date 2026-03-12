#pragma once
#include <memory>
#include <string>
#include "datatype/Pointcloud.h"
#include "datasource/FileDataSource.h"

enum class PointcloudSourceType {
    FILE,
    STREAM
};

class PointcloudFactory {
public:
    static std::shared_ptr<Pointcloud> createPointcloud(PointcloudSourceType type, const std::string& pathOrUri);
};
