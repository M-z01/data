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

class Image : public Data {
private:
    ImageBuffer img;
    ImageFormat format;

public:
    Image(std::shared_ptr<DataSource> src) : Data(src), format(ImageFormat::UNKNOWN) {}
    void load() override;
    void saveToFile(const std::string& path) const;
    const ImageBuffer& getImage() const { return img; }
    ImageFormat getFormat() const { return format; }
};
