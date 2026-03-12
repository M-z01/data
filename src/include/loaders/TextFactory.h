#pragma once
#include <memory>
#include <string>
#include "datatype/Text.h"
#include "datasource/DataSourceFactory.h"

// Backward-compatible alias — existing call sites (TextSourceType::FILE) are unchanged.
using TextSourceType = SourceType;

class TextFactory {
public:
    static std::shared_ptr<Text> createText(TextSourceType type, const std::string& pathOrUri);
};
