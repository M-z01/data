#pragma once
#include <memory>
#include <string>
#include "datatype/Pointcloud.h"
#include "datasource/DataSourceFactory.h"

// Backward-compatible alias — existing call sites (PointcloudSourceType::FILE) are unchanged.
using PointcloudSourceType = SourceType;

class PointcloudFactory {
public:
    static std::shared_ptr<Pointcloud> createPointcloud(PointcloudSourceType type, const std::string& pathOrUri);
};
