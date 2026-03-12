#include "loaders/TextFactory.h"
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

static TextFormat resolveTextFormat(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    if (ext == ".txt")  return TextFormat::TXT;
    if (ext == ".csv")  return TextFormat::CSV;
    if (ext == ".json") return TextFormat::JSON;
    return TextFormat::UNKNOWN;
}

std::shared_ptr<Text> TextFactory::createText(TextSourceType type, const std::string& pathOrUri) {
    std::shared_ptr<DataSource> source;

    switch (type) {
        case TextSourceType::FILE:
            source = std::make_shared<FileDataSource>(pathOrUri);
            break;

        case TextSourceType::STREAM:
            // TODO: Implement stream data source
            throw std::runtime_error("Stream data source not implemented yet");
            break;

        default: throw std::invalid_argument("Unsupported text source type");
    }

    TextFormat fmt = resolveTextFormat(pathOrUri);
    if (fmt == TextFormat::UNKNOWN)
        throw std::runtime_error("Unrecognised text extension: " + pathOrUri);

    return std::make_shared<Text>(source, fmt);
}
