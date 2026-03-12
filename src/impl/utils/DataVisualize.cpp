#include "utils/DataVisualize.h"
#include "utils/OpenCVBridge.h"
#include <iostream>
#include <opencv2/opencv.hpp>

using namespace OpenCVBridge;

// ── public methods ────────────────────────────────────────────────────────────

//Image
void DataVisualize::displayImage(const std::shared_ptr<Image>& img, ImageViewType type) {
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

void DataVisualize::displayPointcloud(const std::shared_ptr<Pointcloud>& pc) {
    // TODO implement this method
}

//Projection
void DataVisualize::projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Image>& img) {
    // TODO implement this method
}

void DataVisualize::projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Pointcloud>& pc) {
    // TODO implement this method
}

