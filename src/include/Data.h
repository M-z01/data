#pragma once
#include <memory>
#include "datasource/DataSource.h"

class Data {
protected:
    std::shared_ptr<DataSource> source; //generic data source, can be file, database, etc.

public:
    Data(std::shared_ptr<DataSource> src) : source(src) {}
    virtual ~Data() {}
    virtual void load() = 0;
};
