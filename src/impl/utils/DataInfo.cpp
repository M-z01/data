#include "utils/DataInfo.h"
#include "utils/OpenCVBridge.h"
#include <iostream>
#include <map>
#include <set>
#include <opencv2/opencv.hpp>

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string formatToString(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::JPG:  return "JPG";
        case ImageFormat::PNG:  return "PNG";
        case ImageFormat::EXR:  return "EXR";
        default:                return "UNKNOWN";
    }
}

static std::string pixelTypeToString(PixelType pt) {
    switch (pt) {
        case PixelType::UINT8:   return "UINT8";
        case PixelType::UINT16:  return "UINT16";
        case PixelType::FLOAT32: return "FLOAT32";
    }
    return "UNKNOWN";
}

// Returns a single-channel float Mat from any ImageBuffer.
static cv::Mat toFloat1ch(const cv::Mat& src) {
    cv::Mat f;
    src.convertTo(f, CV_32F);
    if (f.channels() > 1) {
        std::vector<cv::Mat> ch;
        cv::split(f, ch);
        f = ch[0];
    }
    return f;
}

// Builds a map from unique float value -> sequential integer ID (0 = first).
static std::map<float, int> buildFloatToId(const cv::Mat& f1ch) {
    std::set<float> uniq(f1ch.begin<float>(), f1ch.end<float>());
    std::map<float, int> m;
    int idx = 0;
    for (float v : uniq) m[v] = idx++;
    return m;
}

// ── public methods ────────────────────────────────────────────────────────────

void DataInfo::printImageInfo(const std::shared_ptr<Image>& img, const std::string& type) {
    const ImageBuffer& buf = img->getImage();
    std::cout << "=== Image Info ===\n";
    std::cout << "  Format    : " << formatToString(img->getFormat()) << "\n";
    std::cout << "  Width     : " << buf.width     << " px\n";
    std::cout << "  Height    : " << buf.height    << " px\n";
    std::cout << "  Channels  : " << buf.channels  << "\n";
    std::cout << "  Pixel type: " << pixelTypeToString(buf.pixelType) << "\n";
    std::cout << "  Byte size : " << buf.byteSize() << " bytes\n";

    cv::Mat mat = OpenCVBridge::bufferToMat(buf);

    if (type == "mask") {
        cv::Mat f = toFloat1ch(mat);
        auto float_to_id = buildFloatToId(f);

        // Count pixels per ID
        std::map<int, int> counts;
        for (float v : cv::Mat_<float>(f)) counts[float_to_id[v]]++;

        std::cout << "  Unique mask values and pixel counts:\n";
        for (const auto& [val, uid] : float_to_id)
            std::cout << "    ID " << uid << " (value=" << val
                      << "): " << counts[uid] << " pixels\n";

    } else if (type == "depth") {
        cv::Mat f = toFloat1ch(mat);
        double minVal, maxVal;
        cv::minMaxLoc(f, &minVal, &maxVal);
        std::cout << "  Min depth : " << minVal << "\n";
        std::cout << "  Max depth : " << maxVal << "\n";
    }
    std::cout << "==================\n";
}

void DataInfo::displayImage(const std::shared_ptr<Image>& img, const std::string& type) {
    const ImageBuffer& buf = img->getImage();
    cv::Mat mat = OpenCVBridge::bufferToMat(buf);

    if (type == "mask") {
        cv::Mat f = toFloat1ch(mat);
        auto float_to_id = buildFloatToId(f);

        // Build per-pixel ID map
        cv::Mat id_mat(f.size(), CV_32S);
        for (int r = 0; r < f.rows; ++r)
            for (int c = 0; c < f.cols; ++c)
                id_mat.at<int>(r, c) = float_to_id[f.at<float>(r, c)];

        // Assign random colors (seed 42, matching mask.py)
        cv::RNG rng(42);
        std::map<int, cv::Vec3b> id_colors;
        for (const auto& [val, uid] : float_to_id) {
            id_colors[uid] = (val == 1.0f)
                ? cv::Vec3b(75, 75, 75)   // background (value=1) → gray
                : cv::Vec3b(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
        }

        // Paint color mask
        cv::Mat color_mask(f.size(), CV_8UC3, cv::Scalar(0, 0, 0));
        for (int r = 0; r < id_mat.rows; ++r)
            for (int c = 0; c < id_mat.cols; ++c)
                color_mask.at<cv::Vec3b>(r, c) = id_colors[id_mat.at<int>(r, c)];

        // Overlay UID label at centroid of each object (skip background)
        for (const auto& [val, uid] : float_to_id) {
            if (val == 1.0f) continue;
            cv::Mat bin;
            cv::compare(id_mat, uid, bin, cv::CMP_EQ);
            bin.convertTo(bin, CV_8U);
            cv::Moments m = cv::moments(bin, true);
            if (m.m00 > 0) {
                int cx = static_cast<int>(m.m10 / m.m00);
                int cy = static_cast<int>(m.m01 / m.m00);
                cv::putText(color_mask, std::to_string(uid),
                            cv::Point(cx, cy),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
            }
        }

        cv::imshow("Mask (pseudo-color)", color_mask);
        cv::waitKey(0);

    } else if (type == "depth") {
        cv::Mat f = toFloat1ch(mat);
        double minVal, maxVal;
        cv::minMaxLoc(f, &minVal, &maxVal);

        cv::Mat norm;
        cv::normalize(f, norm, 0, 255, cv::NORM_MINMAX, CV_8U);
        cv::Mat colored;
        cv::applyColorMap(norm, colored, cv::COLORMAP_JET);

        std::string title = "Depth  [min=" + std::to_string(minVal)
                          + "  max=" + std::to_string(maxVal) + "]";
        cv::imshow(title, colored);
        cv::waitKey(0);

    } else {
        // Generic: normalize if needed and display
        cv::Mat display;
        if (mat.depth() == CV_32F || mat.depth() == CV_16U)
            cv::normalize(mat, display, 0, 255, cv::NORM_MINMAX, CV_8U);
        else
            display = mat;
        cv::imshow("Image", display);
        cv::waitKey(0);
    }
}

void DataInfo::printVideoInfo(const std::shared_ptr<Video>& video) {
    // TODO
    // print number of frames, dimensions, format, fps, etc.
}

void DataInfo::printTextInfo(const std::shared_ptr<Text>& text) {
    // TODO
    // IDK yet what info is useful for text data
}