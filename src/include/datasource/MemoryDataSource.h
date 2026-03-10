#pragma once
#include "datasource/DataSource.h"
#include <vector>

// A DataSource backed by an in-memory byte buffer.
// Useful for holding encoded image/video data produced by converters,
// without writing to disk.
class MemoryDataSource : public DataSource {
private:
    std::vector<unsigned char> data;

public:
    explicit MemoryDataSource(std::vector<unsigned char> bytes)
        : data(std::move(bytes)) {}

    std::vector<unsigned char> getRawBytes() override { return data; }
};
