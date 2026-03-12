#include "datatype/Pointcloud.h"
#include <stdexcept>

void Pointcloud::load() {
    if (format == PointcloudFormat::UNKNOWN)
        throw std::runtime_error("Pointcloud format not set before load()");

    const std::vector<unsigned char>& bytes = source->getRawBytes();
    if (bytes.empty()) {
        throw std::runtime_error("Empty data from source");
    }

    // TODO: parse bytes according to format (PCD or PLY)
    loaded_ = true;
}

void Pointcloud::saveToFile(const std::string& path, PointcloudFormat fmt) const {
    // TODO: implement saving pointcloud to file in the appropriate format (PCD, PLY)
}

void Pointcloud::saveToFile(const std::string& path) const {
    saveToFile(path, format);
}