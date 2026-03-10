#pragma once
#include "Data.h"
#include <string>

enum class VideoFormat {
    MP4,
    AVI,
    MKV,
    UNKNOWN
};

class Video : public Data {
private:
    std::vector<cv::Mat> frames;
    VideoFormat format;

public:
    Video(std::shared_ptr<DataSource> src) : Data(src), format(VideoFormat::UNKNOWN) {}
    void load() override;
    const std::vector<cv::Mat>& getFrames() const { return frames; }
    VideoFormat getFormat() const { return format; }
};
