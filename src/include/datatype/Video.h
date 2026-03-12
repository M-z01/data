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

inline std::string toString(VideoFormat fmt) {
    switch (fmt) {
        case VideoFormat::MP4: return "MP4";
        case VideoFormat::AVI: return "AVI";
        case VideoFormat::MKV: return "MKV";
        default:               return "UNKNOWN";
    }
}

class Video : public Data {
private:
    std::vector<ImageBuffer> frames;
    VideoFormat format;
    double fps;

public:
    Video(std::shared_ptr<DataSource> src, VideoFormat fmt = VideoFormat::UNKNOWN) : Data(src), format(fmt), fps(0.0) {}
    void load() override;
    void saveToFile(const std::string& path) const override;
    void saveToFile(const std::string& path, VideoFormat fmt) const;
    const std::vector<ImageBuffer>& getFrames() const { return frames; }
    VideoFormat getFormat() const { return format; }
    double getFps() const { return fps; }
    void setFps(double newFps) { fps = newFps; }
};
