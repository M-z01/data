#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include "datasource/DataSource.h"

class Data {
protected:
    std::shared_ptr<DataSource> source; // generic data source, can be file, database, etc.
    bool loaded_ = false;

public:
    explicit Data(std::shared_ptr<DataSource> src) : source(std::move(src)) {}
    virtual ~Data() = default;
    virtual void load() = 0;
    virtual void saveToFile(const std::string& path) const = 0;

    /// Returns true after load() has been called (and before any unload/reset).
    bool isLoaded() const { return loaded_; }

protected:
    /// Guard used by saveToFile() implementations: throws if not yet loaded.
    void requireLoaded(const std::string& typeName) const {
        if (!loaded_)
            throw std::runtime_error("Cannot save " + typeName + ": call load() first");
    }
};
