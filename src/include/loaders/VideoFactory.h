#pragma once
#include <memory>
#include <string>
#include "datatype/Video.h"
#include "datasource/FileDataSource.h"

enum class VideoSourceType {
    FILE,
    STREAM
};

class VideoFactory {
public:
    static std::shared_ptr<Video> createVideo(VideoSourceType type, const std::string& pathOrUri);
};
