#pragma once
#include "datasource/DataSource.h"

class ROS2BagDataSource : public DataSource {
    private :
        std::string bagPath;

    public :
        ROS2BagDataSource(const std::string& path) : bagPath(path) {}
        std::vector<unsigned char> getRawBytes() override;
        std::string getPath() const { return bagPath; }
};