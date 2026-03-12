#pragma once
#include <memory>
#include <string>
#include <vector>

class DataSource {
    public:
        virtual ~DataSource() = default;
        // Returns a reference to the raw byte buffer — zero-copy for cached sources.
        virtual const std::vector<unsigned char>& getRawBytes() = 0;

        // Returns the filesystem path if this source is file-backed, empty string otherwise.
        // Allows consumers (e.g. Video::load) to open the file directly via FFmpeg
        // instead of buffering the entire encoded file into heap memory.
        virtual std::string getPath() const { return ""; }
};
