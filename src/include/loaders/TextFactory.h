#pragma once
#include <memory>
#include <string>
#include "datatype/Text.h"
#include "datasource/FileDataSource.h"

enum class TextSourceType {
    FILE,
    STREAM
};

class TextFactory {
public:
    static std::shared_ptr<Text> createText(TextSourceType type, const std::string& pathOrUri);
};
