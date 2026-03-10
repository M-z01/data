// Internal-only header – do NOT include from public headers.
// Provides helpers to convert between cv::Mat and ImageBuffer.
#pragma once
#include <opencv2/opencv.hpp>
#include "datatype/ImageBuffer.h"

namespace OpenCVBridge {

inline ImageBuffer matToBuffer(const cv::Mat& mat) {
    ImageBuffer buf;
    buf.width    = mat.cols;
    buf.height   = mat.rows;
    buf.channels = mat.channels();

    switch (mat.depth()) {
        case CV_8U:  buf.pixelType = PixelType::UINT8;   break;
        case CV_16U: buf.pixelType = PixelType::UINT16;  break;
        case CV_32F: buf.pixelType = PixelType::FLOAT32; break;
        default:     buf.pixelType = PixelType::UINT8;   break;
    }

    // Ensure the data is in a contiguous block
    cv::Mat contiguous = mat.isContinuous() ? mat : mat.clone();
    const uint8_t* begin = contiguous.data;
    const uint8_t* end   = begin + contiguous.total() * contiguous.elemSize();
    buf.data.assign(begin, end);
    return buf;
}

inline cv::Mat bufferToMat(const ImageBuffer& buf) {
    int depth = CV_8U;
    switch (buf.pixelType) {
        case PixelType::UINT8:   depth = CV_8U;  break;
        case PixelType::UINT16:  depth = CV_16U; break;
        case PixelType::FLOAT32: depth = CV_32F; break;
    }
    int type = CV_MAKETYPE(depth, buf.channels);
    cv::Mat mat(buf.height, buf.width, type);
    std::memcpy(mat.data, buf.data.data(), buf.data.size());
    return mat;
}

} // namespace OpenCVBridge
