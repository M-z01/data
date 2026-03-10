#pragma once
#include "Data.h"

enum class ImageFormat {
    JPG,
    PNG,
    EXR,
    UNKNOWN
};

class Image : public Data {
private:
    cv::Mat img;
    ImageFormat format;

public:
    Image(std::shared_ptr<DataSource> src) : Data(src), format(ImageFormat::UNKNOWN) {}
    void load() override;
    cv::Mat getImage() const { return img; }
    ImageFormat getFormat() const { return format; }
};
