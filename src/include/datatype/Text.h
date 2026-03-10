#pragma once
#include "Data.h"
#include <string>

enum class TextFormat {
    TXT,
    CSV,
    JSON,
    UNKNOWN
};

class Text : public Data {
private:
    std::string content;
    TextFormat format;

public:
    Text(std::shared_ptr<DataSource> src) : Data(src), format(TextFormat::UNKNOWN) {}
    void load() override;
    const std::string& getContent() const { return content; }
    TextFormat getFormat() const { return format; }
};
