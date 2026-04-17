#include "utils/DataVisualize.h"
#include "utils/OpenCVBridge.h"
#include <iostream>
#include <cmath>
#include <array>
#include <opencv2/viz.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ── private helpers for 3D bbox projection ────────────────────────────────────

// Quaternion (w,x,y,z) → 3×3 rotation matrix  (matches Python quat_wxyz_to_rot3)
static std::array<std::array<double,3>,3> quatToRot(double w, double x, double y, double z) {
    double n = std::sqrt(w*w + x*x + y*y + z*z);
    if (n == 0) throw std::runtime_error("Zero-length quaternion");
    w /= n; x /= n; y /= n; z /= n;
    return {{
        {{ 1 - 2*(y*y + z*z),     2*(x*y - z*w),     2*(x*z + y*w) }},
        {{     2*(x*y + z*w), 1 - 2*(x*x + z*z),     2*(y*z - x*w) }},
        {{     2*(x*z - y*w),     2*(y*z + x*w), 1 - 2*(x*x + y*y) }}
    }};
}

// Multiply 3×3 rotation by a column vector and add translation
static std::array<double,3> transform(const std::array<std::array<double,3>,3>& R,
                                       const std::array<double,3>& v,
                                       const std::array<double,3>& t) {
    return {{
        R[0][0]*v[0] + R[0][1]*v[1] + R[0][2]*v[2] + t[0],
        R[1][0]*v[0] + R[1][1]*v[1] + R[1][2]*v[2] + t[1],
        R[2][0]*v[0] + R[2][1]*v[1] + R[2][2]*v[2] + t[2]
    }};
}

// Get the 8 corners of a 3D oriented bounding box  (matches Python get_3d_bbox_corners)
static std::array<std::array<double,3>,8> getBBoxCorners(
        const std::array<double,3>& center,
        const std::array<double,4>& quat_wxyz,
        const std::array<double,3>& extents) {
    auto R = quatToRot(quat_wxyz[0], quat_wxyz[1], quat_wxyz[2], quat_wxyz[3]);
    double hx = extents[0] / 2.0, hy = extents[1] / 2.0, hz = extents[2] / 2.0;
    std::array<std::array<double,3>,8> local = {{
        {{-hx,-hy,-hz}}, {{ hx,-hy,-hz}}, {{ hx, hy,-hz}}, {{-hx, hy,-hz}},
        {{-hx,-hy, hz}}, {{ hx,-hy, hz}}, {{ hx, hy, hz}}, {{-hx, hy, hz}}
    }};
    std::array<std::array<double,3>,8> world;
    for (int i = 0; i < 8; ++i)
        world[i] = transform(R, local[i], center);
    return world;
}

// Project a 3D point to 2D pixel coordinates  (matches Python project_3d_to_2d)
static cv::Point2d project3dTo2d(const std::array<double,3>& pt,
                                  double fx, double fy, double cx, double cy) {
    double z = (pt[2] == 0.0) ? 1e-6 : pt[2];
    return { fx * pt[0] / z + cx, fy * pt[1] / z + cy };
}

// Draw a transparent filled polygon onto img  (matches Python draw_transparent_polygon)
static void drawTransparentPoly(cv::Mat& img, const std::vector<cv::Point>& pts,
                                 const cv::Scalar& color, double alpha) {
    cv::Mat overlay = img.clone();
    cv::fillPoly(overlay, std::vector<std::vector<cv::Point>>{pts}, color);
    cv::addWeighted(overlay, alpha, img, 1.0 - alpha, 0, img);
}

// Draw 3D bounding-box projection with transparent faces and layered edges
// (matches Python draw_bbox)
static void drawBBox(cv::Mat& img, const std::array<cv::Point,8>& pts,
                     const cv::Scalar& color, int thickness, double alpha) {
    // 6 faces
    const int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {3,2,6,7}, {0,3,7,4}, {1,2,6,5}
    };
    for (auto& f : faces) {
        std::vector<cv::Point> poly = { pts[f[0]], pts[f[1]], pts[f[2]], pts[f[3]] };
        drawTransparentPoly(img, poly, color, alpha);
    }
    // Layered edge colors
    cv::Scalar cGround(color[0]*0.3, color[1]*0.3, color[2]*0.3);
    cv::Scalar cPillar(color[0]*0.6, color[1]*0.6, color[2]*0.6);

    // Ground edges
    for (auto [i,j] : std::initializer_list<std::pair<int,int>>{{0,1},{1,5},{5,4},{4,0}})
        cv::line(img, pts[i], pts[j], cGround, thickness);
    // Pillar edges
    for (auto [i,j] : std::initializer_list<std::pair<int,int>>{{0,3},{1,2},{4,7},{5,6}})
        cv::line(img, pts[i], pts[j], cPillar, thickness);
    // Top edges
    for (auto [i,j] : std::initializer_list<std::pair<int,int>>{{2,3},{3,7},{7,6},{6,2}})
        cv::line(img, pts[i], pts[j], color, thickness);
}

// Draw coordinate axes at object center (matches Python draw_axes)
static void drawAxes(cv::Mat& img,
                     const std::array<double,3>& center,
                     const std::array<std::array<double,3>,3>& R,
                     double fx, double fy, double cx, double cy,
                     double axisLen, int thickness) {
    std::array<double,3> zero = {0,0,0};
    std::array<double,3> axX = {axisLen,0,0}, axY = {0,axisLen,0}, axZ = {0,0,axisLen};
    auto origin = project3dTo2d(transform(R, zero, center), fx, fy, cx, cy);
    auto pX     = project3dTo2d(transform(R, axX,  center), fx, fy, cx, cy);
    auto pY     = project3dTo2d(transform(R, axY,  center), fx, fy, cx, cy);
    auto pZ     = project3dTo2d(transform(R, axZ,  center), fx, fy, cx, cy);

    cv::Point o(int(origin.x), int(origin.y));
    cv::line(img, o, {int(pX.x),int(pX.y)}, {0,0,255},   thickness); // X = red
    cv::line(img, o, {int(pY.x),int(pY.y)}, {0,255,0},   thickness); // Y = green
    cv::line(img, o, {int(pZ.x),int(pZ.y)}, {255,0,0},   thickness); // Z = blue
}

// Parse the metadata JSON (from Text content) and return structured data
struct ObjectMeta {
    std::string          name;
    std::array<double,3> translation;
    std::array<double,4> quat_wxyz;   // w,x,y,z
    std::array<double,3> bbox_extents;
};
struct FrameMeta {
    double fx, fy, cx, cy;
    int    width, height;
    std::vector<ObjectMeta> objects;
};

static FrameMeta parseMetaJson(const std::string& jsonStr) {
    json j = json::parse(jsonStr);
    FrameMeta fm;
    auto& intr = j["camera"]["intrinsics"];
    fm.fx = intr["fx"].get<double>();
    fm.fy = intr["fy"].get<double>();
    fm.cx = intr["cx"].get<double>();
    fm.cy = intr["cy"].get<double>();
    fm.width  = intr.value("width",  0);
    fm.height = intr.value("height", 0);

    if (j.contains("objects")) {
        for (auto& [key, val] : j["objects"].items()) {
            ObjectMeta om;
            om.name = key;
            auto& t = val["translation"];
            om.translation = {{ t[0].get<double>(), t[1].get<double>(), t[2].get<double>() }};
            // Support both "quaternion_wxyz" and "quaternion" keys
            auto& q = val.contains("quaternion_wxyz") ? val["quaternion_wxyz"] : val["quaternion"];
            om.quat_wxyz = {{ q[0].get<double>(), q[1].get<double>(), q[2].get<double>(), q[3].get<double>() }};
            auto& bsl = val["meta"]["bbox_side_len"];
            om.bbox_extents = {{ bsl[0].get<double>(), bsl[1].get<double>(), bsl[2].get<double>() }};
            fm.objects.push_back(std::move(om));
        }
    }
    return fm;
}

// ── mouse-click callback for pixel inspection ───────────────────────────────
namespace {

enum class ClickMode { GENERIC, DEPTH, MASK, SEGMENTATION };

struct ClickData {
    ClickMode                   mode;
    cv::Mat                     rawMat;    // primary data (float for DEPTH/MASK, raw for GENERIC)
    cv::Mat                     auxMat;    // original BGR display (used by SEGMENTATION)
    const std::map<float, int>* floatToId; // for MASK & SEGMENTATION
};

static void onMouseClick(int event, int x, int y, int /*flags*/, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN) return;
    auto* d = static_cast<ClickData*>(userdata);
    if (!d || x < 0 || y < 0 || x >= d->rawMat.cols || y >= d->rawMat.rows) return;
    std::cout << "[Click] (" << x << ", " << y << ")  ";
    switch (d->mode) {
    case ClickMode::DEPTH:
        std::cout << "depth = " << d->rawMat.at<float>(y, x) << "\n";
        break;
    case ClickMode::MASK: {
        float val = d->rawMat.at<float>(y, x);
        std::cout << "mask_value = " << val;
        if (d->floatToId) {
            auto it = d->floatToId->find(val);
            if (it != d->floatToId->end())
                std::cout << "  uid = " << it->second;
        }
        std::cout << "\n";
        break;
    }
    case ClickMode::SEGMENTATION: {
        float mval = d->rawMat.at<float>(y, x);
        std::cout << "mask_value = " << mval;
        if (d->floatToId) {
            auto it = d->floatToId->find(mval);
            if (it != d->floatToId->end())
                std::cout << "  uid = " << it->second;
        }
        if (!d->auxMat.empty() && y < d->auxMat.rows && x < d->auxMat.cols
                && d->auxMat.channels() == 3 && d->auxMat.depth() == CV_8U) {
            cv::Vec3b px = d->auxMat.at<cv::Vec3b>(y, x);
            std::cout << "  BGR = (" << (int)px[0] << ", " << (int)px[1] << ", " << (int)px[2] << ")";
        }
        std::cout << "\n";
        break;
    }
    default: { // GENERIC
        int ch  = d->rawMat.channels();
        int dep = d->rawMat.depth();
        if (ch == 1) {
            if      (dep == CV_32F)  std::cout << "value = " << d->rawMat.at<float>(y, x);
            else if (dep == CV_16U)  std::cout << "value = " << d->rawMat.at<uint16_t>(y, x);
            else                     std::cout << "value = " << (int)d->rawMat.at<uint8_t>(y, x);
        } else if (ch == 3) {
            if (dep == CV_8U) {
                cv::Vec3b px = d->rawMat.at<cv::Vec3b>(y, x);
                std::cout << "BGR = (" << (int)px[0] << ", " << (int)px[1] << ", " << (int)px[2] << ")";
            } else if (dep == CV_32F) {
                cv::Vec3f px = d->rawMat.at<cv::Vec3f>(y, x);
                std::cout << "BGR = (" << px[0] << ", " << px[1] << ", " << px[2] << ")";
            } else if (dep == CV_16U) {
                cv::Vec3w px = d->rawMat.at<cv::Vec3w>(y, x);
                std::cout << "BGR = (" << px[0] << ", " << px[1] << ", " << px[2] << ")";
            }
        } else if (ch == 4 && dep == CV_8U) {
            cv::Vec4b px = d->rawMat.at<cv::Vec4b>(y, x);
            std::cout << "BGRA = (" << (int)px[0] << ", " << (int)px[1] << ", "
                      << (int)px[2] << ", " << (int)px[3] << ")";
        }
        std::cout << "\n";
        break;
    }
    }
}

} // anonymous namespace

// ── public methods ────────────────────────────────────────────────────────────

//Image
void DataVisualize::displayImage(const std::shared_ptr<Image>& img, ImageViewType type,
                                  const std::shared_ptr<Image>& mask, bool denormMask) {
    const ImageBuffer& buf = img->getImage();
    cv::Mat mat = OpenCVBridge::bufferToMat(buf);

    // ── If a segmentation mask is provided, overlay it on the image ──────────
    if (mask) {
        if (mask->getImage().empty()) mask->load();
        cv::Mat maskF = OpenCVBridge::toFloat1ch(
            OpenCVBridge::bufferToMat(mask->getImage()));
        if (denormMask) maskF = OpenCVBridge::denormalizeMask(maskF);
        auto float_to_id = OpenCVBridge::buildFloatToId(maskF);
        const float bgVal = denormMask ? 255.0f : 1.0f;

        // Build colour map for each segment (background → transparent)
        cv::RNG rng(42);
        std::map<float, cv::Vec3b> float_to_color;
        for (const auto& [val, uid] : float_to_id) {
            if (val == bgVal) continue; // skip background
            float_to_color[val] = cv::Vec3b(rng.uniform(0, 255),
                                             rng.uniform(0, 255),
                                             rng.uniform(0, 255));
        }

        // Prepare base image as 8-bit BGR
        cv::Mat display;
        if (mat.channels() == 1) cv::cvtColor(mat, display, cv::COLOR_GRAY2BGR);
        else                     display = mat.clone();
        if (display.depth() != CV_8U) {
            cv::Mat tmp;
            cv::normalize(display, tmp, 0, 255, cv::NORM_MINMAX, CV_8U);
            display = tmp;
        }

        // Blend segmentation colour overlay onto foreground pixels
        cv::Mat overlay = display.clone();
        for (int r = 0; r < maskF.rows && r < display.rows; ++r) {
            const float* mRow = maskF.ptr<float>(r);
            cv::Vec3b*   oRow = overlay.ptr<cv::Vec3b>(r);
            for (int c = 0; c < maskF.cols && c < display.cols; ++c) {
                auto it = float_to_color.find(mRow[c]);
                if (it != float_to_color.end())
                    oRow[c] = it->second;
            }
        }
        cv::addWeighted(overlay, 0.4, display, 0.6, 0, display);

        // Draw UID labels at centroids
        for (const auto& [val, uid] : float_to_id) {
            if (val == bgVal) continue;
            cv::Mat bin;
            cv::compare(maskF, val, bin, cv::CMP_EQ);
            cv::Moments m = cv::moments(bin, true);
            if (m.m00 > 0) {
                int cx = static_cast<int>(m.m10 / m.m00);
                int cy = static_cast<int>(m.m01 / m.m00);
                std::string label = denormMask ? std::to_string(static_cast<int>(val))
                                               : std::to_string(uid);
                cv::putText(display, label,
                            cv::Point(cx, cy),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
            }
        }

        cv::imshow("Image + Segmentation", display);
        ClickData clickData{ClickMode::SEGMENTATION, maskF, display, &float_to_id};
        cv::setMouseCallback("Image + Segmentation", onMouseClick, &clickData);
        cv::waitKey(0);
        cv::setMouseCallback("Image + Segmentation", nullptr, nullptr);
        return;
    }

    if (type == ImageViewType::MASK) {
        cv::Mat f = OpenCVBridge::toFloat1ch(mat);
        if (denormMask) f = OpenCVBridge::denormalizeMask(f);
        auto float_to_id = OpenCVBridge::buildFloatToId(f);
        const float bgVal = denormMask ? 255.0f : 1.0f;

        // Build direct float→color map so we only need one pass over the image.
        cv::RNG rng(42);
        std::map<float, cv::Vec3b> float_to_color;
        for (const auto& [val, uid] : float_to_id) {
            float_to_color[val] = (val == bgVal)
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
            if (val == bgVal) continue;
            cv::Mat bin;
            cv::compare(f, val, bin, cv::CMP_EQ);
            cv::Moments m = cv::moments(bin, true);
            if (m.m00 > 0) {
                int cx = static_cast<int>(m.m10 / m.m00);
                int cy = static_cast<int>(m.m01 / m.m00);
                std::string label = denormMask ? std::to_string(static_cast<int>(val))
                                               : std::to_string(uid);
                cv::putText(color_mask, label,
                            cv::Point(cx, cy),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
            }
        }

        cv::imshow("Mask (pseudo-color)", color_mask);
        ClickData clickData{ClickMode::MASK, f, {}, &float_to_id};
        cv::setMouseCallback("Mask (pseudo-color)", onMouseClick, &clickData);
        cv::waitKey(0);
        cv::setMouseCallback("Mask (pseudo-color)", nullptr, nullptr);

    } else if (type == ImageViewType::DEPTH) {
        cv::Mat f = OpenCVBridge::toFloat1ch(mat);
        double minVal, maxVal;
        cv::minMaxLoc(f, &minVal, &maxVal);

        cv::Mat norm;
        cv::normalize(f, norm, 0, 255, cv::NORM_MINMAX, CV_8U);
        cv::Mat colored;
        cv::applyColorMap(norm, colored, cv::COLORMAP_JET);

        std::string title = "Depth  [min=" + std::to_string(minVal)
                          + "  max=" + std::to_string(maxVal) + "]";
        cv::imshow(title, colored);
        ClickData clickData{ClickMode::DEPTH, f, {}, nullptr};
        cv::setMouseCallback(title, onMouseClick, &clickData);
        cv::waitKey(0);
        cv::setMouseCallback(title, nullptr, nullptr);

    } else {
        // Generic: normalize if needed and display
        cv::Mat display;
        if (mat.depth() == CV_32F || mat.depth() == CV_16U)
            cv::normalize(mat, display, 0, 255, cv::NORM_MINMAX, CV_8U);
        else
            display = mat;
        cv::imshow("Image", display);
        ClickData clickData{ClickMode::GENERIC, mat, {}, nullptr};
        cv::setMouseCallback("Image", onMouseClick, &clickData);
        cv::waitKey(0);
        cv::setMouseCallback("Image", nullptr, nullptr);
    }
}

void DataVisualize::displayPointcloud(const std::shared_ptr<Pointcloud>& pc,
                                       const std::shared_ptr<Image>& mask,
                                       const std::shared_ptr<Image>& rgb,
                                       const std::shared_ptr<Image>& depth) {
    // ── If mask is given, filter points using the segmentation mask ──────────
    if (mask) {
        if (mask->getImage().empty()) mask->load();
        cv::Mat maskF = OpenCVBridge::toFloat1ch(
            OpenCVBridge::bufferToMat(mask->getImage()));

        // Build per-segment colour map (background 1.0 → skip)
        auto float_to_id = OpenCVBridge::buildFloatToId(maskF);
        cv::RNG rng(42);
        std::map<float, cv::Vec3b> seg_color;
        for (const auto& [val, uid] : float_to_id) {
            if (val == 1.0f) continue;
            seg_color[val] = cv::Vec3b(rng.uniform(0, 255),
                                        rng.uniform(0, 255),
                                        rng.uniform(0, 255));
        }

        // ── If rgb + depth are also given, back-project from images ──────
        if (rgb && depth) {
            if (rgb->getImage().empty())   rgb->load();
            if (depth->getImage().empty()) depth->load();

            cv::Mat rgbMat = OpenCVBridge::bufferToMat(rgb->getImage());
            cv::Mat depthMat = OpenCVBridge::bufferToMat(depth->getImage());

            if (rgbMat.depth() != CV_8U)
                rgbMat.convertTo(rgbMat, CV_8U,
                    rgbMat.depth() == CV_16U ? 1.0 / 256.0 : 255.0);

            cv::Mat depthF;
            if (depthMat.depth() == CV_32F)      depthF = depthMat;
            else if (depthMat.depth() == CV_16U) depthMat.convertTo(depthF, CV_32F, 1.0 / 1000.0);
            else                                 depthMat.convertTo(depthF, CV_32F);
            if (depthF.channels() > 1) {
                std::vector<cv::Mat> ch; cv::split(depthF, ch); depthF = ch[0];
            }

            const int w = rgbMat.cols;
            const int h = rgbMat.rows;
            const float fx = static_cast<float>(w);
            const float fy = static_cast<float>(w);
            const float cx = static_cast<float>(w) * 0.5f;
            const float cy = static_cast<float>(h) * 0.5f;

            std::vector<cv::Vec3f> filteredPts;
            std::vector<cv::Vec3b> filteredCols;

            for (int v = 0; v < h; ++v) {
                const float* mRow = maskF.ptr<float>(v);
                const float* dRow = depthF.ptr<float>(v);
                for (int u = 0; u < w; ++u) {
                    if (mRow[u] == 1.0f) continue;
                    const float z = dRow[u];
                    if (z <= 0.0f || !std::isfinite(z)) continue;

                    const float x = (static_cast<float>(u) - cx) * z / fx;
                    const float y = (static_cast<float>(v) - cy) * z / fy;
                    filteredPts.push_back(cv::Vec3f(x, y, z));

                    auto it = seg_color.find(mRow[u]);
                    if (it != seg_color.end()) {
                        const cv::Vec3b& bgr = rgbMat.at<cv::Vec3b>(v, u);
                        cv::Vec3b blended(
                            static_cast<uint8_t>(bgr[0] * 0.5 + it->second[0] * 0.5),
                            static_cast<uint8_t>(bgr[1] * 0.5 + it->second[1] * 0.5),
                            static_cast<uint8_t>(bgr[2] * 0.5 + it->second[2] * 0.5));
                        filteredCols.push_back(blended);
                    } else {
                        filteredCols.push_back(rgbMat.at<cv::Vec3b>(v, u));
                    }
                }
            }

            if (filteredPts.empty()) {
                std::cerr << "[displayPointcloud] No foreground points after mask filtering.\n";
                return;
            }

            cv::Mat cloud(1, static_cast<int>(filteredPts.size()), CV_32FC3,
                          filteredPts.data());
            cv::Mat colors(1, static_cast<int>(filteredCols.size()), CV_8UC3,
                           filteredCols.data());

            cv::viz::Viz3d viewer("Segmented Point Cloud");
            viewer.setBackgroundColor(cv::viz::Color::black());
            cv::viz::WCloud cloudWidget(cloud, colors);
            cloudWidget.setRenderingProperty(cv::viz::POINT_SIZE, 2.0);
            viewer.showWidget("cloud", cloudWidget);

            double minV, maxV;
            cv::minMaxLoc(cloud.reshape(1), &minV, &maxV);
            double axisSize = std::max(0.05, (maxV - minV) * 0.1);
            viewer.showWidget("axes", cv::viz::WCoordinateSystem(axisSize));

            std::cout << "[displayPointcloud] Showing " << filteredPts.size()
                      << " segmented points (from RGB-D). Close the window to continue.\n";
            viewer.spin();
            return;
        }

        // ── Mask-only: project existing 3D points onto the mask image ────
        const auto& pts = pc->getPoints();
        const size_t nPts = pts.size() / 3;
        if (nPts == 0) {
            std::cerr << "[displayPointcloud] Empty point cloud, nothing to show.\n";
            return;
        }

        const int mW = maskF.cols;
        const int mH = maskF.rows;
        const float fx = static_cast<float>(mW);
        const float fy = static_cast<float>(mW);
        const float cx = static_cast<float>(mW) * 0.5f;
        const float cy = static_cast<float>(mH) * 0.5f;

        // Existing colours (if any)
        const bool hasCols = pc->hasColors();
        const auto& colData = pc->getColors();
        const int cStride = pc->hasAlpha() ? 4 : 3;

        std::vector<cv::Vec3f> filteredPts;
        std::vector<cv::Vec3b> filteredCols;

        for (size_t i = 0; i < nPts; ++i) {
            float x = pts[i * 3 + 0];
            float y = pts[i * 3 + 1];
            float z = pts[i * 3 + 2];
            if (z <= 0.0f || !std::isfinite(z)) continue;

            // Project 3D → 2D pixel
            int u = static_cast<int>(fx * x / z + cx);
            int v = static_cast<int>(fy * y / z + cy);
            if (u < 0 || u >= mW || v < 0 || v >= mH) continue;

            float mVal = maskF.at<float>(v, u);
            if (mVal == 1.0f) continue; // background

            filteredPts.push_back(cv::Vec3f(x, y, z));

            // Blend existing colour with segment colour
            cv::Vec3b baseCol(255, 255, 255);
            if (hasCols) {
                // Stored as RGB, viz expects BGR
                baseCol = cv::Vec3b(colData[i * cStride + 2],
                                    colData[i * cStride + 1],
                                    colData[i * cStride + 0]);
            }
            auto it = seg_color.find(mVal);
            if (it != seg_color.end()) {
                cv::Vec3b blended(
                    static_cast<uint8_t>(baseCol[0] * 0.5 + it->second[0] * 0.5),
                    static_cast<uint8_t>(baseCol[1] * 0.5 + it->second[1] * 0.5),
                    static_cast<uint8_t>(baseCol[2] * 0.5 + it->second[2] * 0.5));
                filteredCols.push_back(blended);
            } else {
                filteredCols.push_back(baseCol);
            }
        }

        if (filteredPts.empty()) {
            std::cerr << "[displayPointcloud] No foreground points after mask filtering.\n";
            return;
        }

        cv::Mat cloud(1, static_cast<int>(filteredPts.size()), CV_32FC3,
                      filteredPts.data());
        cv::Mat colors(1, static_cast<int>(filteredCols.size()), CV_8UC3,
                       filteredCols.data());

        cv::viz::Viz3d viewer("Segmented Point Cloud");
        viewer.setBackgroundColor(cv::viz::Color::black());
        cv::viz::WCloud cloudWidget(cloud, colors);
        cloudWidget.setRenderingProperty(cv::viz::POINT_SIZE, 2.0);
        viewer.showWidget("cloud", cloudWidget);

        double minV, maxV;
        cv::minMaxLoc(cloud.reshape(1), &minV, &maxV);
        double axisSize = std::max(0.05, (maxV - minV) * 0.1);
        viewer.showWidget("axes", cv::viz::WCoordinateSystem(axisSize));

        std::cout << "[displayPointcloud] Showing " << filteredPts.size()
                  << " segmented points (of " << nPts << " total). Close the window to continue.\n";
        viewer.spin();
        return;
    }

    const auto& pts  = pc->getPoints();
    const size_t nPts = pts.size() / 3;
    if (nPts == 0) {
        std::cerr << "[displayPointcloud] Empty point cloud, nothing to show.\n";
        return;
    }

    // Build Nx1 3-channel float Mat for cv::viz (XYZ)
    cv::Mat cloud(1, static_cast<int>(nPts), CV_32FC3);
    std::memcpy(cloud.data, pts.data(), nPts * 3 * sizeof(float));

    // Build colour Mat if available
    cv::Mat colors;
    if (pc->hasColors()) {
        const auto& colData = pc->getColors();
        const int stride = pc->hasAlpha() ? 4 : 3;
        colors = cv::Mat(1, static_cast<int>(nPts), CV_8UC3);
        auto* dst = colors.ptr<cv::Vec3b>();
        const uint8_t* src = colData.data();
        for (size_t i = 0; i < nPts; ++i) {
            // Input is RGB(A), OpenCV viz expects BGR
            dst[i] = cv::Vec3b(src[i*stride+2], src[i*stride+1], src[i*stride+0]);
        }
    }

    // Create the 3D viewer
    cv::viz::Viz3d viewer("Point Cloud Viewer");
    viewer.setBackgroundColor(cv::viz::Color::black());

    cv::viz::WCloud cloudWidget = colors.empty()
        ? cv::viz::WCloud(cloud, cv::viz::Color::white())
        : cv::viz::WCloud(cloud, colors);
    cloudWidget.setRenderingProperty(cv::viz::POINT_SIZE, 2.0);
    viewer.showWidget("cloud", cloudWidget);

    // Coordinate frame at origin (size = 10 % of cloud extent)
    double minV, maxV;
    cv::minMaxLoc(cloud.reshape(1), &minV, &maxV);
    double axisSize = std::max(0.05, (maxV - minV) * 0.1);
    viewer.showWidget("axes", cv::viz::WCoordinateSystem(axisSize));

    std::cout << "[displayPointcloud] Showing " << nPts << " points. Close the window to continue.\n";
    viewer.spin();
}

//Projection

void DataVisualize::projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Image>& img) {
    // Parse the metadata JSON stored in the Text object
    FrameMeta meta = parseMetaJson(text->getContent());

    // Convert Image buffer to an OpenCV Mat for drawing
    cv::Mat mat = OpenCVBridge::bufferToMat(img->getImage());

    // Ensure we have a 3-channel 8-bit image for colour drawing
    if (mat.channels() == 1)
        cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
    if (mat.depth() != CV_8U) {
        cv::Mat tmp;
        cv::normalize(mat, tmp, 0, 255, cv::NORM_MINMAX, CV_8U);
        mat = tmp;
    }

    const cv::Scalar bboxColor(255, 0, 255); // magenta (BGR)

    for (const auto& obj : meta.objects) {
        // Compute the 8 corners of the oriented 3D bounding box
        auto corners3d = getBBoxCorners(obj.translation, obj.quat_wxyz, obj.bbox_extents);

        // Project each corner to 2D
        std::array<cv::Point,8> corners2d;
        for (int i = 0; i < 8; ++i) {
            auto p = project3dTo2d(corners3d[i], meta.fx, meta.fy, meta.cx, meta.cy);
            corners2d[i] = cv::Point(static_cast<int>(p.x), static_cast<int>(p.y));
        }

        // Draw transparent faces + layered edges
        drawBBox(mat, corners2d, bboxColor, /*thickness=*/2, /*alpha=*/0.05);

        // Draw coordinate axes at the object centre
        auto R = quatToRot(obj.quat_wxyz[0], obj.quat_wxyz[1],
                           obj.quat_wxyz[2], obj.quat_wxyz[3]);
        drawAxes(mat, obj.translation, R,
                 meta.fx, meta.fy, meta.cx, meta.cy,
                 /*axisLen=*/0.03, /*thickness=*/2);

        // Put the object name near the projected centre
        auto ctr2d = project3dTo2d(obj.translation, meta.fx, meta.fy, meta.cx, meta.cy);
        cv::putText(mat, obj.name,
                    cv::Point(static_cast<int>(ctr2d.x), static_cast<int>(ctr2d.y) - 8),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }

    // Store the drawn result back into the Image
    img->setImage(OpenCVBridge::matToBuffer(mat));

    // Display the result
    cv::imshow("BBox Projection", mat);
    cv::waitKey(0);
}

void DataVisualize::projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Pointcloud>& pc) {
    // Parse the metadata JSON stored in the Text object
    FrameMeta meta = parseMetaJson(text->getContent());

    // Start from the existing point cloud data
    std::vector<float>   pts   = pc->getPoints();
    std::vector<uint8_t> cols  = pc->getColors();
    bool                 alpha = pc->hasAlpha();
    const int            cStride = alpha ? 4 : 3;

    // If there are no existing colours, initialise to white
    if (cols.empty()) {
        size_t nPts = pts.size() / 3;
        cols.resize(nPts * cStride, 255);
    }

    // Colour for bbox wireframe points (red)
    const uint8_t lineR = 255, lineG = 0, lineB = 0;

    // Number of interpolation samples along each bbox edge
    constexpr int edgeSamples = 30;

    // 12 edges of a box (same adjacency as Open3D LineSet)
    static const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},   // bottom face
        {4,5},{5,6},{6,7},{7,4},   // top face
        {0,4},{1,5},{2,6},{3,7}    // pillars
    };

    for (const auto& obj : meta.objects) {
        auto corners = getBBoxCorners(obj.translation, obj.quat_wxyz, obj.bbox_extents);

        // Add interpolated points along each edge
        for (auto [i,j] : edges) {
            for (int s = 0; s <= edgeSamples; ++s) {
                double t = static_cast<double>(s) / edgeSamples;
                double px = corners[i][0] + t * (corners[j][0] - corners[i][0]);
                double py = corners[i][1] + t * (corners[j][1] - corners[i][1]);
                double pz = corners[i][2] + t * (corners[j][2] - corners[i][2]);
                pts.push_back(static_cast<float>(px));
                pts.push_back(static_cast<float>(py));
                pts.push_back(static_cast<float>(pz));
                cols.push_back(lineR);
                cols.push_back(lineG);
                cols.push_back(lineB);
                if (alpha) cols.push_back(255);
            }
        }

        // Add small sphere-like cluster at each corner for visibility
        constexpr double cornerRadius = 0.003; // 3 mm
        for (int c = 0; c < 8; ++c) {
            for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz) {
                pts.push_back(static_cast<float>(corners[c][0] + dx * cornerRadius));
                pts.push_back(static_cast<float>(corners[c][1] + dy * cornerRadius));
                pts.push_back(static_cast<float>(corners[c][2] + dz * cornerRadius));
                cols.push_back(lineR);
                cols.push_back(lineG);
                cols.push_back(lineB);
                if (alpha) cols.push_back(255);
            }
        }
    }

    // Write the augmented data back into the Pointcloud object
    pc->setPoints(std::move(pts));
    pc->setColors(std::move(cols), alpha);

    std::cout << "[ProjectTextContent] Added bbox wireframes for "
              << meta.objects.size() << " object(s) to point cloud ("
              << pc->getPoints().size() / 3 << " total points)\n";

    // Visualize the result (matches image projection behaviour)
    displayPointcloud(pc);
}

