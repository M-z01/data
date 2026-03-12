// DataSourceFactory.h — unified data-source type enum and factory helper.
//
// All four loader headers (ImageFactory, VideoFactory, TextFactory,
// PointcloudFactory) pull in this header and define their per-type alias
// (e.g. `using ImageSourceType = SourceType;`) so existing call sites
// continue to compile without change.
#pragma once
#include <memory>
#include <string>
#include <stdexcept>
#include "datasource/DataSource.h"
#include "datasource/FileDataSource.h"

/// Unified source-type enum shared by all data factories.
enum class SourceType {
    FILE,
    STREAM
};

/// Create the appropriate DataSource for the given type and path/URI.
/// Removes the identical FILE/STREAM switch from every factory .cpp.
inline std::shared_ptr<DataSource> createDataSource(SourceType type,
                                                     const std::string& pathOrUri) {
    switch (type) {
        case SourceType::FILE:
            return std::make_shared<FileDataSource>(pathOrUri);
        case SourceType::STREAM:
            throw std::runtime_error("Stream data source not implemented yet");
        default:
            throw std::invalid_argument("Unsupported source type");
    }
}
