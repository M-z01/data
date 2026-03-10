#pragma once
#include <vector>
#include <cstdint>

// Pixel element type (applies to every channel in the image)
enum class PixelType {
    UINT8,   // 1 byte per channel  (8-bit JPG / PNG)
    UINT16,  // 2 bytes per channel (16-bit PNG)
    FLOAT32  // 4 bytes per channel (EXR)
};

// A simple, OpenCV-free image buffer.
// Data is stored row-major, interleaved channels (same memory layout as cv::Mat).
struct ImageBuffer {
    std::vector<uint8_t> data; // raw pixel bytes
    int width    = 0;
    int height   = 0;
    int channels = 0;
    PixelType pixelType = PixelType::UINT8;

    bool empty() const { return data.empty() || width == 0 || height == 0; }

    // Size in bytes of one channel element
    int elementSize() const {
        switch (pixelType) {
            case PixelType::UINT8:   return 1;
            case PixelType::UINT16:  return 2;
            case PixelType::FLOAT32: return 4;
        }
        return 1;
    }

    // Total byte size
    std::size_t byteSize() const {
        return static_cast<std::size_t>(width) * height * channels * elementSize();
    }
};
