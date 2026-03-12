#include "datasource/FileDataSource.h"
#include <fstream>
#include <stdexcept>

const std::vector<unsigned char>& FileDataSource::getRawBytes() {
    if (cacheLoaded_) return cache_;
    std::ifstream file(filePath, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open file: " + filePath);
    cache_.assign(std::istreambuf_iterator<char>(file),
                  std::istreambuf_iterator<char>());
    cacheLoaded_ = true;
    return cache_;
}