#pragma once
#include "Data.h"
#include <stdexcept>
#include <string>

enum class TextFormat {
    TXT,
    CSV,
    JSON,
    UNKNOWN
};

inline std::string toString(TextFormat fmt) {
    switch (fmt) {
        case TextFormat::TXT:  return "TXT";
        case TextFormat::CSV:  return "CSV";
        case TextFormat::JSON: return "JSON";
        default:               return "UNKNOWN";
    }
}

inline TextFormat textFormatFromString(const std::string& s) {
    if (s == "TXT")  return TextFormat::TXT;
    if (s == "CSV")  return TextFormat::CSV;
    if (s == "JSON") return TextFormat::JSON;
    throw std::invalid_argument("Unknown text format: " + s);
}

class Text : public Data {
private:
    std::string content;
    TextFormat format;

public:
    Text(std::shared_ptr<DataSource> src, TextFormat fmt = TextFormat::UNKNOWN) : Data(src), format(fmt) {}
    void load() override;
    void saveToFile(const std::string& path) const override;
    void saveToFile(const std::string& path, TextFormat fmt) const;
    const std::string& getContent() const { return content; }
    TextFormat getFormat() const { return format; }
};
