#pragma once
#include "Data.h"
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
