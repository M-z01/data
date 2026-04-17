// Internal-only header – do NOT include from public headers.
// Provides helpers to convert between cv::Mat and ImageBuffer.
#pragma once
#include <map>
#include <set>
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

// Returns a single-channel float Mat from any Mat (any depth/channels).
inline cv::Mat toFloat1ch(const cv::Mat& src) {
    cv::Mat f;
    src.convertTo(f, CV_32F);
    if (f.channels() > 1) {
        std::vector<cv::Mat> ch;
        cv::split(f, ch);
        f = ch[0];
    }
    return f;
}

// Multiply each pixel by 255 and round to nearest integer (for denormalizing
// masks that were stored in [0, 1] instead of [0, 255]).
inline cv::Mat denormalizeMask(const cv::Mat& f1ch) {
    cv::Mat out;
    f1ch.convertTo(out, CV_32F, 255.0);
    for (float& v : cv::Mat_<float>(out))
        v = std::round(v);
    return out;
}

// Builds a map from unique float value -> sequential integer ID (0 = first).
inline std::map<float, int> buildFloatToId(const cv::Mat& f1ch) {
    std::set<float> uniq(f1ch.begin<float>(), f1ch.end<float>());
    std::map<float, int> m;
    int idx = 0;
    for (float v : uniq) m[v] = idx++;
    return m;
}

} // namespace OpenCVBridge
