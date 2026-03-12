#pragma once
#include "Data.h"
#include <stdexcept>
#include <string>
#include <vector>

enum class PointcloudFormat {
    PCD,
    PLY,
    UNKNOWN
};

inline std::string toString(PointcloudFormat fmt) {
    switch (fmt) {
        case PointcloudFormat::PCD: return "PCD";
        case PointcloudFormat::PLY: return "PLY";
        default:                    return "UNKNOWN";
    }
}

inline PointcloudFormat pointcloudFormatFromString(const std::string& s) {
    if (s == "PCD") return PointcloudFormat::PCD;
    if (s == "PLY") return PointcloudFormat::PLY;
    throw std::invalid_argument("Unknown pointcloud format: " + s);
}

class Pointcloud : public Data {
private:
    std::vector<float>   points; // flat: x1,y1,z1, x2,y2,z2, ...
    // Per-point colour in RGBA order.  When hasAlpha_ is false each point
    // occupies 3 bytes (r,g,b); when true it occupies 4 bytes (r,g,b,a).
    std::vector<uint8_t> colors;
    bool                 hasAlpha_ = false;
    PointcloudFormat     format;

public:
    Pointcloud(std::shared_ptr<DataSource> src, PointcloudFormat fmt = PointcloudFormat::UNKNOWN)
        : Data(src), format(fmt) {}

    void load() override;
    void saveToFile(const std::string& path) const override;
    void saveToFile(const std::string& path, PointcloudFormat fmt) const;

    // ── geometry ─────────────────────────────────────────────────────────────
    const std::vector<float>& getPoints() const { return points; }
    /// Directly set the parsed points (skips re-parsing when already available).
    void setPoints(std::vector<float> pts) { points = std::move(pts); loaded_ = true; }

    // ── colour ────────────────────────────────────────────────────────────────
    bool hasColors() const { return !colors.empty(); }
    bool hasAlpha()  const { return hasAlpha_; }
    /// Returns flat byte array: 3 bytes/point (r,g,b) or 4 bytes/point (r,g,b,a).
    const std::vector<uint8_t>& getColors() const { return colors; }
    /// Set per-point colour data.  @p alpha should be true when each point has 4 bytes.
    void setColors(std::vector<uint8_t> c, bool alpha = false) {
        colors = std::move(c); hasAlpha_ = alpha;
    }

    PointcloudFormat getFormat() const { return format; }
};

