#include "loaders/PointcloudFactory.h"
#include "utils/FormatDetector.h"
#include <stdexcept>

std::shared_ptr<Pointcloud> PointcloudFactory::createPointcloud(PointcloudSourceType type, const std::string& pathOrUri) {
    auto source = createDataSource(type, pathOrUri);

    const std::string fmtStr = FormatDetector::pointcloudFormat(pathOrUri);
    if (fmtStr.empty())
        throw std::runtime_error("Unrecognised pointcloud extension: " + pathOrUri);

    PointcloudFormat fmt = PointcloudFormat::UNKNOWN;
    if      (fmtStr == "PCD") fmt = PointcloudFormat::PCD;
    else if (fmtStr == "PLY") fmt = PointcloudFormat::PLY;

    return std::make_shared<Pointcloud>(source, fmt);
}
