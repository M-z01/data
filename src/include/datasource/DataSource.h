#pragma once
#include <memory>
#include <string>
#include <opencv2/opencv.hpp>

class DataSource {
    public:
        virtual ~DataSource() {}
        virtual std::vector<unsigned char> getRawBytes() = 0;
};
