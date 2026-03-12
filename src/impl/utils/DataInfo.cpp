#include "utils/DataInfo.h"
#include "utils/OpenCVBridge.h"
#include <iostream>
#include <map>
#include <set>
#include <opencv2/opencv.hpp>

// ── helpers ──────────────────────────────────────────────────────────────────

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

//Image
void DataInfo::printImageInfo(const std::shared_ptr<Image>& img, ImageViewType type) {
    const ImageBuffer& buf = img->getImage();
    std::cout << "=== Image Info ===\n";
    std::cout << "  Format    : " << toString(img->getFormat()) << "\n";
    std::cout << "  Width     : " << buf.width     << " px\n";
    std::cout << "  Height    : " << buf.height    << " px\n";
    std::cout << "  Channels  : " << buf.channels  << "\n";
    std::cout << "  Pixel type: " << toString(buf.pixelType) << "\n";
    std::cout << "  Byte size : " << buf.byteSize() << " bytes\n";

    cv::Mat mat = OpenCVBridge::bufferToMat(buf);

    if (type == ImageViewType::MASK) {
        cv::Mat f = toFloat1ch(mat);
        auto float_to_id = buildFloatToId(f);

        // Count pixels per ID
        std::map<int, int> counts;
        for (float v : cv::Mat_<float>(f)) counts[float_to_id[v]]++;

        std::cout << "  Unique mask values and pixel counts:\n";
        for (const auto& [val, uid] : float_to_id)
            std::cout << "    ID " << uid << " (value=" << val
                      << "): " << counts[uid] << " pixels\n";

    } else if (type == ImageViewType::DEPTH) {
        cv::Mat f = toFloat1ch(mat);
        double minVal, maxVal;
        cv::minMaxLoc(f, &minVal, &maxVal);
        std::cout << "  Min depth : " << minVal << "\n";
        std::cout << "  Max depth : " << maxVal << "\n";
    }
    std::cout << "==================\n";
}

void DataInfo::displayImage(const std::shared_ptr<Image>& img, ImageViewType type) {
    const ImageBuffer& buf = img->getImage();
    cv::Mat mat = OpenCVBridge::bufferToMat(buf);

    if (type == ImageViewType::MASK) {
        cv::Mat f = toFloat1ch(mat);
        auto float_to_id = buildFloatToId(f);

        // Build direct float→color map so we only need one pass over the image.
        cv::RNG rng(42);
        std::map<float, cv::Vec3b> float_to_color;
        for (const auto& [val, uid] : float_to_id) {
            float_to_color[val] = (val == 1.0f)
                ? cv::Vec3b(75, 75, 75)
                : cv::Vec3b(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
        }

        // Single-pass coloring via raw row pointers (no id_mat intermediate).
        cv::Mat color_mask(f.size(), CV_8UC3, cv::Scalar(0, 0, 0));
        for (int r = 0; r < f.rows; ++r) {
            const float* fRow = f.ptr<float>(r);
            cv::Vec3b*   cRow = color_mask.ptr<cv::Vec3b>(r);
            for (int c = 0; c < f.cols; ++c)
                cRow[c] = float_to_color[fRow[c]];
        }

        // Overlay UID label at centroid of each object (skip background).
        // cv::compare + cv::moments are already vectorised internally.
        for (const auto& [val, uid] : float_to_id) {
            if (val == 1.0f) continue;
            cv::Mat bin;
            cv::compare(f, val, bin, cv::CMP_EQ);
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

    } else if (type == ImageViewType::DEPTH) {
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

//Video
void DataInfo::printVideoInfo(const std::shared_ptr<Video>& video) {
    const auto& frames = video->getFrames();
    std::cout << "=== Video Info ===\n";
    std::cout << "  Format    : " << toString(video->getFormat()) << "\n";
    std::cout << "  FPS       : " << video->getFps() << "\n";
    if (!frames.empty()) {
        // Full info available after load()
        const ImageBuffer& f = frames[0];
        std::cout << "  Frames    : " << frames.size() << "\n";
        std::cout << "  Width     : " << f.width     << " px\n";
        std::cout << "  Height    : " << f.height    << " px\n";
        std::cout << "  Channels  : " << f.channels  << "\n";
        std::cout << "  Pixel type: " << toString(f.pixelType) << "\n";
    } else {
        // Lightweight metadata available after scanMetadata()
        std::cout << "  Frames    : (streaming \u2014 count unknown)\n";
        if (video->getWidth() > 0) {
            std::cout << "  Width     : " << video->getWidth()    << " px\n";
            std::cout << "  Height    : " << video->getHeight()   << " px\n";
            std::cout << "  Channels  : " << video->getChannels() << "\n";
        }
    }
    std::cout << "==================\n";
}

//Text
void DataInfo::printTextInfo(const std::shared_ptr<Text>& text) {
    const std::string& content = text->getContent();
    std::cout << "=== Text Info ===\n";
    std::cout << "  Format    : " << toString(text->getFormat()) << "\n";
    std::cout << "  Length    : " << content.size() << " bytes\n";
    std::cout << "  Lines     : " << std::count(content.begin(), content.end(), '\n') + (content.empty() ? 0 : 1) << "\n";
    std::cout << "=================\n";
}

//Text + Image
void DataInfo::projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Image>& img) {
    
}

//Text + Pointcloud
void DataInfo::projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Pointcloud>& pc) {
    
}