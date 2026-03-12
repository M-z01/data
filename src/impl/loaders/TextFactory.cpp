#include "loaders/TextFactory.h"
#include "utils/FormatDetector.h"
#include <stdexcept>

std::shared_ptr<Text> TextFactory::createText(TextSourceType type, const std::string& pathOrUri) {
    auto source = createDataSource(type, pathOrUri);

    const std::string fmtStr = FormatDetector::textFormat(pathOrUri);
    if (fmtStr.empty())
        throw std::runtime_error("Unrecognised text extension: " + pathOrUri);

    TextFormat fmt = TextFormat::UNKNOWN;
    if      (fmtStr == "TXT")  fmt = TextFormat::TXT;
    else if (fmtStr == "CSV")  fmt = TextFormat::CSV;
    else if (fmtStr == "JSON") fmt = TextFormat::JSON;

    return std::make_shared<Text>(source, fmt);
}
