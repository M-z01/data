#pragma once
#include "datasource/DataSource.h"

class FileDataSource : public DataSource {
    private :
        std::string filePath;

    public :
        FileDataSource(const std::string& path) : filePath(path) {}
        std::vector<unsigned char> getRawBytes() override;
        std::string getPath() const { return filePath; }
};
