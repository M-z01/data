#pragma once
#include "Data.h"
#include "datatype/ImageBuffer.h"
#include <string>
#include <vector>
#include <functional>

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
    // Lightweight metadata set by scanMetadata() without decoding all frames.
    int metaWidth    = 0;
    int metaHeight   = 0;
    int metaChannels = 0;

public:
    Video(std::shared_ptr<DataSource> src, VideoFormat fmt = VideoFormat::UNKNOWN) : Data(src), format(fmt), fps(0.0) {}

    // Decodes all frames into memory (existing eager-load path).
    void load() override;

    // Lightweight metadata scan: opens the source, reads stream info to detect
    // fps/dimensions by decoding exactly one frame. Does NOT populate frames[].
    // Call this instead of load() when you only need metadata before a
    // forEachFrame() streaming pass.
    void scanMetadata();

    // Streaming decode: invokes cb(frameIndex, buffer) for each frame one at a time.
    // Only one frame's pixel data is in RAM at a time — use this instead of load()
    // when you cannot afford to hold all frames simultaneously.
    using FrameCallback = std::function<void(std::size_t, const ImageBuffer&)>;
    void forEachFrame(FrameCallback cb) const;

    void saveToFile(const std::string& path) const override;
    void saveToFile(const std::string& path, VideoFormat fmt) const;
    const std::vector<ImageBuffer>& getFrames() const { return frames; }
    VideoFormat getFormat() const { return format; }
    double getFps() const { return fps; }
    void setFps(double newFps) { fps = newFps; }
    // Metadata available after scanMetadata() or load()
    int getWidth()    const { return metaWidth;    }
    int getHeight()   const { return metaHeight;   }
    int getChannels() const { return metaChannels; }
};
