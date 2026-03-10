#pragma once
#include "Data.h"
#include "datatype/ImageBuffer.h"
#include <string>
#include <vector>

enum class VideoFormat {
    MP4,
    AVI,
    MKV,
    UNKNOWN
};

class Video : public Data {
private:
    std::vector<ImageBuffer> frames;
    VideoFormat format;
    double fps;

public:
    Video(std::shared_ptr<DataSource> src) : Data(src), format(VideoFormat::UNKNOWN), fps(0.0) {}
    void load() override;
    void saveToFile(const std::string& path) const;
    const std::vector<ImageBuffer>& getFrames() const { return frames; }
    VideoFormat getFormat() const { return format; }
    double getFps() const { return fps; }
    void setFps(double newFps) { fps = newFps; }
};
