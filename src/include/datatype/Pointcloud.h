#pragma once
#include "Data.h"
#include <string>

enum class PointcloudFormat {
    PCD,
    PLY,
    UNKNOWN
};

inline std::string toString(PointcloudFormat fmt) {
    switch (fmt) {
        case PointcloudFormat::PCD: return "PCD";
        case PointcloudFormat::PLY: return "PLY";
        default:                    return "UNKNOWN";
    }
}

class Pointcloud : public Data {
private:
    std::vector<float> points; // flat array: x1,y1,z1,x2,y2,z2,...
    PointcloudFormat format;

public:
    Pointcloud(std::shared_ptr<DataSource> src, PointcloudFormat fmt = PointcloudFormat::UNKNOWN) : Data(src), format(fmt) {}
    void load() override;
    void saveToFile(const std::string& path) const override;
    void saveToFile(const std::string& path, PointcloudFormat fmt) const;
    const std::vector<float>& getPoints() const { return points; }
    PointcloudFormat getFormat() const { return format; }
};

