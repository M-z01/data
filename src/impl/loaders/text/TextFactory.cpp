#include "loaders/text/TextFactory.h"
#include <stdexcept>

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

    return std::make_shared<Text>(source);
}
