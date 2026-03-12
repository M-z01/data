#pragma once
#include <memory>
#include <string>
#include "datatype/Video.h"
#include "datasource/DataSourceFactory.h"

// Backward-compatible alias — existing call sites (VideoSourceType::FILE) are unchanged.
using VideoSourceType = SourceType;

class VideoFactory {
public:
    static std::shared_ptr<Video> createVideo(VideoSourceType type, const std::string& pathOrUri);
};
