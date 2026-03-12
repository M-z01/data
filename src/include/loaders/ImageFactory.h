#pragma once
#include <memory>
#include <string>
#include "datatype/Image.h"
#include "datasource/DataSourceFactory.h"

// Backward-compatible alias — existing call sites (ImageSourceType::FILE) are unchanged.
using ImageSourceType = SourceType;

class ImageFactory {
public:
    static std::shared_ptr<Image> createImage(ImageSourceType type, const std::string& pathOrUri);
};
