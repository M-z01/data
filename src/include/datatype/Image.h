#pragma once
#include "Data.h"

class Image : public Data {
private:
    cv::Mat img;

public:
    Image(std::shared_ptr<DataSource> src) : Data(src) {}
    void load() override;
    cv::Mat getImage() const { return img; }
};
