#pragma once
#include "Data.h"
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
    void setPoints(const std::vector<float>& pts) { points = pts; loaded_ = true; }

    // ── colour ────────────────────────────────────────────────────────────────
    bool hasColors() const { return !colors.empty(); }
    bool hasAlpha()  const { return hasAlpha_; }
    /// Returns flat byte array: 3 bytes/point (r,g,b) or 4 bytes/point (r,g,b,a).
    const std::vector<uint8_t>& getColors() const { return colors; }
    /// Set per-point colour data.  @p alpha should be true when each point has 4 bytes.
    void setColors(const std::vector<uint8_t>& c, bool alpha = false) {
        colors = c; hasAlpha_ = alpha;
    }

    PointcloudFormat getFormat() const { return format; }
};

