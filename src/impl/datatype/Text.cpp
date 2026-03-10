#include "datatype/Text.h"
#include <stdexcept>

static TextFormat detectFormat(const std::string& content) {
    // Skip leading whitespace
    size_t start = content.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return TextFormat::UNKNOWN;

    char first = content[start];
    if (first == '{' || first == '[') {
        return TextFormat::JSON;
    }

    // Heuristic: if any line contains a comma, treat as CSV
    size_t newline = content.find('\n');
    std::string firstLine = (newline != std::string::npos) ? content.substr(0, newline) : content;
    if (firstLine.find(',') != std::string::npos) {
        return TextFormat::CSV;
    }

    return TextFormat::TXT;
}

void Text::load() {
    std::vector<unsigned char> bytes = source->getRawBytes();
    if (bytes.empty()) {
        throw std::runtime_error("Empty data from source");
    }

    content = std::string(bytes.begin(), bytes.end());
    format  = detectFormat(content);
}
