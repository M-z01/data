#pragma once
#include <memory>
#include <string>
#include <vector>

class DataSource {
    public:
        virtual ~DataSource() {}
        virtual std::vector<unsigned char> getRawBytes() = 0;
};
