#include "datatype/Text.h"
#include <stdexcept>
#include <fstream>

void Text::load() {
    if (format == TextFormat::UNKNOWN)
        throw std::runtime_error("Text format not set before load()");

    const std::vector<unsigned char>& bytes = source->getRawBytes();
    if (bytes.empty()) {
        throw std::runtime_error("Empty data from source");
    }

    content.assign(bytes.begin(), bytes.end());
    loaded_ = true;
}

void Text::saveToFile(const std::string& path, TextFormat fmt) const {
    requireLoaded("Text");
    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("Failed to open file for writing: " + path);
    out << content;
    if (!out)
        throw std::runtime_error("Failed to write text to file: " + path);
}

void Text::saveToFile(const std::string& path) const {
    saveToFile(path, format);
}
