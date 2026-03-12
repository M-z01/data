#pragma once
#include "datasource/DataSource.h"

class FileDataSource : public DataSource {
    private:
        std::string filePath;
        mutable std::vector<unsigned char> cache_;
        mutable bool cacheLoaded_ = false;

    public:
        explicit FileDataSource(const std::string& path) : filePath(path) {}
        // Reads the file on first call; subsequent calls return the cached buffer.
        const std::vector<unsigned char>& getRawBytes() override;
        std::string getPath() const { return filePath; }
};
