#pragma once
#include "Data.h"
#include "datatype/ImageBuffer.h"
#include <string>

enum class ImageFormat {
    JPG,
    PNG,
    EXR,
    UNKNOWN
};

inline std::string toString(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::JPG:  return "JPG";
        case ImageFormat::PNG:  return "PNG";
        case ImageFormat::EXR:  return "EXR";
        default:                return "UNKNOWN";
    }
}

class Image : public Data {
private:
    ImageBuffer img;
    ImageFormat format;

public:
    Image(std::shared_ptr<DataSource> src, ImageFormat fmt = ImageFormat::UNKNOWN) : Data(src), format(fmt) {}
    void load() override;
    // Releases decoded pixel data from RAM while keeping the DataSource intact.
    // The image can be re-loaded by calling load() again.
    void unload() { img = ImageBuffer{}; loaded_ = false; }
    // Directly set the decoded buffer (skips re-decoding when the cv::Mat is already available).
    void setImage(ImageBuffer buf) { img = std::move(buf); loaded_ = true; }
    void saveToFile(const std::string& path) const override;
    void saveToFile(const std::string& path, ImageFormat fmt, PixelType pixelType) const;
    const ImageBuffer& getImage() const { return img; }
    ImageFormat getFormat() const { return format; }
};
