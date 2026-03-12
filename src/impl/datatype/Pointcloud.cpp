#include "datatype/Pointcloud.h"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

// ── Colour helpers ────────────────────────────────────────────────────────────

/// Unpack a PCD "rgb" or "rgba" packed-float field into individual bytes.
/// PCD stores the 3/4 bytes as the lower bits of an IEEE-754 float.
static void unpackPCDRGBA(float packed, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    uint32_t raw;
    std::memcpy(&raw, &packed, sizeof(raw));
    b = static_cast<uint8_t>((raw >>  0) & 0xFF);
    g = static_cast<uint8_t>((raw >>  8) & 0xFF);
    r = static_cast<uint8_t>((raw >> 16) & 0xFF);
    a = static_cast<uint8_t>((raw >> 24) & 0xFF);
}

static float packRGBtoFloat(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t raw = (static_cast<uint32_t>(r) << 16) |
                   (static_cast<uint32_t>(g) <<  8) |
                   (static_cast<uint32_t>(b));
    float f;
    std::memcpy(&f, &raw, sizeof(f));
    return f;
}

static float packRGBAtoFloat(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint32_t raw = (static_cast<uint32_t>(a) << 24) |
                   (static_cast<uint32_t>(r) << 16) |
                   (static_cast<uint32_t>(g) <<  8) |
                   (static_cast<uint32_t>(b));
    float f;
    std::memcpy(&f, &raw, sizeof(f));
    return f;
}

// ── PCD parser ────────────────────────────────────────────────────────────────

struct PointcloudData {
    std::vector<float>   points;  // x,y,z per point
    std::vector<uint8_t> colors;  // r,g,b or r,g,b,a per point
    bool hasAlpha = false;
};

static PointcloudData parsePCD(const std::vector<unsigned char>& bytes) {
    const char* data = reinterpret_cast<const char*>(bytes.data());
    const size_t size = bytes.size();
    size_t pos = 0;

    auto readLine = [&]() -> std::string {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') ++pos;
        std::string line(data + start, pos - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (pos < size) ++pos;
        return line;
    };

    int numPoints = 0;
    std::string dataType;
    std::vector<std::string> fields;
    std::vector<int> sizes;
    std::vector<int> counts;

    while (pos < size) {
        std::string line = readLine();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "FIELDS") {
            std::string f;
            while (iss >> f) fields.push_back(f);
        } else if (key == "SIZE") {
            int s;
            while (iss >> s) sizes.push_back(s);
        } else if (key == "COUNT") {
            int c;
            while (iss >> c) counts.push_back(c);
        } else if (key == "POINTS") {
            iss >> numPoints;
        } else if (key == "DATA") {
            iss >> dataType;
            break;
        }
    }

    int xi = -1, yi = -1, zi = -1;
    // PCD packs colour as a single "rgb" or "rgba" float field
    int rgbI = -1, rgbaI = -1;
    // Separate r/g/b/a channels (less common but valid)
    int ri = -1, gi = -1, bi = -1, ai = -1;
    for (int i = 0; i < (int)fields.size(); ++i) {
        if      (fields[i] == "x")    xi    = i;
        else if (fields[i] == "y")    yi    = i;
        else if (fields[i] == "z")    zi    = i;
        else if (fields[i] == "rgb")  rgbI  = i;
        else if (fields[i] == "rgba") rgbaI = i;
        else if (fields[i] == "r")    ri    = i;
        else if (fields[i] == "g")    gi    = i;
        else if (fields[i] == "b")    bi    = i;
        else if (fields[i] == "a")    ai    = i;
    }
    if (xi < 0 || yi < 0 || zi < 0)
        throw std::runtime_error("PCD file missing x/y/z fields");

    // Decide colour mode
    bool colorPacked = (rgbaI >= 0 || rgbI >= 0);
    bool colorSep    = (ri >= 0 && gi >= 0 && bi >= 0);
    bool hasAlpha    = (rgbaI >= 0) || (colorSep && ai >= 0);
    int  colorCh     = hasAlpha ? 4 : 3;

    PointcloudData result;
    result.hasAlpha = hasAlpha;
    result.points.reserve(numPoints * 3);
    if (colorPacked || colorSep)
        result.colors.reserve(numPoints * colorCh);

    const int maxGeomIdx = std::max({xi, yi, zi});
    const int packedCI   = (rgbaI >= 0) ? rgbaI : rgbI;
    const int maxColorIdx = colorSep
        ? (hasAlpha ? std::max({ri,gi,bi,ai}) : std::max({ri,gi,bi}))
        : 0;

    if (dataType == "ascii") {
        for (int n = 0; n < numPoints && pos < size; ++n) {
            std::string line = readLine();
            if (line.empty()) { --n; continue; }
            std::istringstream iss(line);
            std::vector<float> vals;
            float v;
            while (iss >> v) vals.push_back(v);
            if ((int)vals.size() <= maxGeomIdx) continue;
            result.points.push_back(vals[xi]);
            result.points.push_back(vals[yi]);
            result.points.push_back(vals[zi]);
            if (colorPacked) {
                if (packedCI < (int)vals.size()) {
                    // ASCII PCD writes the packed color as a plain decimal integer.
                    // Reading it via `float` preserves the value exactly for 24-bit
                    // colors (integers <= 2^24 are exactly representable), so we
                    // recover the original uint32 with static_cast — NOT memcpy.
                    uint32_t packed_int = static_cast<uint32_t>(static_cast<long long>(vals[packedCI]));
                    uint8_t r = (packed_int >> 16) & 0xFF;
                    uint8_t g = (packed_int >>  8) & 0xFF;
                    uint8_t b = (packed_int >>  0) & 0xFF;
                    uint8_t a = (packed_int >> 24) & 0xFF;
                    result.colors.push_back(r);
                    result.colors.push_back(g);
                    result.colors.push_back(b);
                    if (hasAlpha) result.colors.push_back(a);
                }
            } else if (colorSep) {
                if ((int)vals.size() > maxColorIdx) {
                    result.colors.push_back(static_cast<uint8_t>(vals[ri]));
                    result.colors.push_back(static_cast<uint8_t>(vals[gi]));
                    result.colors.push_back(static_cast<uint8_t>(vals[bi]));
                    if (hasAlpha) result.colors.push_back(static_cast<uint8_t>(vals[ai]));
                }
            }
        }
    } else if (dataType == "binary") {
        // Compute per-field byte offsets using SIZE and COUNT
        std::vector<int> offsets(fields.size(), 0);
        int off = 0;
        for (int i = 0; i < (int)fields.size(); ++i) {
            offsets[i] = off;
            off += sizes[i] * (counts.empty() ? 1 : counts[i]);
        }
        int stride = off;
        const unsigned char* base = bytes.data() + pos;
        for (int n = 0; n < numPoints; ++n) {
            const unsigned char* pt = base + n * stride;
            float x, y, z;
            std::memcpy(&x, pt + offsets[xi], sizeof(float));
            std::memcpy(&y, pt + offsets[yi], sizeof(float));
            std::memcpy(&z, pt + offsets[zi], sizeof(float));
            result.points.push_back(x);
            result.points.push_back(y);
            result.points.push_back(z);
            if (colorPacked) {
                float packed;
                std::memcpy(&packed, pt + offsets[packedCI], sizeof(float));
                uint8_t r, g, b, a;
                unpackPCDRGBA(packed, r, g, b, a);
                result.colors.push_back(r);
                result.colors.push_back(g);
                result.colors.push_back(b);
                if (hasAlpha) result.colors.push_back(a);
            } else if (colorSep) {
                auto readU8fromField = [&](int fi) -> uint8_t {
                    if (sizes[fi] == 1) return *(pt + offsets[fi]);
                    if (sizes[fi] == 4) {
                        float fv; std::memcpy(&fv, pt + offsets[fi], 4);
                        return static_cast<uint8_t>(fv);
                    }
                    return 0;
                };
                result.colors.push_back(readU8fromField(ri));
                result.colors.push_back(readU8fromField(gi));
                result.colors.push_back(readU8fromField(bi));
                if (hasAlpha) result.colors.push_back(readU8fromField(ai));
            }
        }
    } else {
        throw std::runtime_error("PCD data type not supported: " + dataType);
    }
    return result;
}

// ── PLY parser ────────────────────────────────────────────────────────────────

static PointcloudData parsePLY(const std::vector<unsigned char>& bytes) {
    const char* data = reinterpret_cast<const char*>(bytes.data());
    const size_t size = bytes.size();
    size_t pos = 0;

    auto readLine = [&]() -> std::string {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') ++pos;
        std::string line(data + start, pos - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (pos < size) ++pos;
        return line;
    };

    std::string plyFormat;
    int numVertices = 0;
    // Track property name + type for each vertex property
    struct PropInfo { std::string name; std::string type; };
    std::vector<PropInfo> props;
    bool inVertexElement = false;

    while (pos < size) {
        std::string line = readLine();
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        if (token == "format") {
            iss >> plyFormat;
        } else if (token == "element") {
            std::string elem;
            iss >> elem;
            if (elem == "vertex") {
                iss >> numVertices;
                inVertexElement = true;
            } else {
                inVertexElement = false;
            }
        } else if (token == "property" && inVertexElement) {
            std::string type, name;
            iss >> type >> name;
            props.push_back({name, type});
        } else if (token == "end_header") {
            break;
        }
    }

    int xi = -1, yi = -1, zi = -1;
    int ri = -1, gi = -1, bi = -1, ai = -1;
    for (int i = 0; i < (int)props.size(); ++i) {
        if      (props[i].name == "x")                              xi = i;
        else if (props[i].name == "y")                              yi = i;
        else if (props[i].name == "z")                              zi = i;
        else if (props[i].name == "red"   || props[i].name == "r") ri = i;
        else if (props[i].name == "green" || props[i].name == "g") gi = i;
        else if (props[i].name == "blue"  || props[i].name == "b") bi = i;
        else if (props[i].name == "alpha" || props[i].name == "a") ai = i;
    }
    if (xi < 0 || yi < 0 || zi < 0)
        throw std::runtime_error("PLY file missing x/y/z vertex properties");

    bool hasColor = (ri >= 0 && gi >= 0 && bi >= 0);
    bool hasAlpha = hasColor && (ai >= 0);
    int  colorCh  = hasAlpha ? 4 : 3;

    PointcloudData result;
    result.hasAlpha = hasAlpha;
    result.points.reserve(numVertices * 3);
    if (hasColor) result.colors.reserve(numVertices * colorCh);

    const int maxGeomIdx  = std::max({xi, yi, zi});
    const int maxColorIdx = hasColor
        ? (hasAlpha ? std::max({ri,gi,bi,ai}) : std::max({ri,gi,bi}))
        : 0;

    if (plyFormat == "ascii") {
        for (int n = 0; n < numVertices && pos < size; ++n) {
            std::string line = readLine();
            std::istringstream iss(line);
            std::vector<float> vals;
            float v;
            while (iss >> v) vals.push_back(v);
            if ((int)vals.size() <= maxGeomIdx) continue;
            result.points.push_back(vals[xi]);
            result.points.push_back(vals[yi]);
            result.points.push_back(vals[zi]);
            if (hasColor) {
                if ((int)vals.size() > maxColorIdx) {
                    result.colors.push_back(static_cast<uint8_t>(vals[ri]));
                    result.colors.push_back(static_cast<uint8_t>(vals[gi]));
                    result.colors.push_back(static_cast<uint8_t>(vals[bi]));
                    if (hasAlpha) result.colors.push_back(static_cast<uint8_t>(vals[ai]));
                }
            }
        }
    } else if (plyFormat == "binary_little_endian" || plyFormat == "binary_big_endian") {
        bool needSwap = (plyFormat == "binary_big_endian");

        // Build per-property byte offsets from the type declarations
        auto typeSize = [](const std::string& t) -> int {
            if (t == "char" || t == "uchar" || t == "int8" || t == "uint8") return 1;
            if (t == "short" || t == "ushort" || t == "int16" || t == "uint16") return 2;
            if (t == "int" || t == "uint" || t == "int32" || t == "uint32" ||
                t == "float" || t == "float32") return 4;
            if (t == "double" || t == "float64" || t == "int64" || t == "uint64") return 8;
            return 4;
        };
        std::vector<int> offsets(props.size());
        std::vector<int> propSizes(props.size());
        int off = 0;
        for (int i = 0; i < (int)props.size(); ++i) {
            offsets[i]   = off;
            propSizes[i] = typeSize(props[i].type);
            off         += propSizes[i];
        }
        int stride = off;
        const unsigned char* base = bytes.data() + pos;

        auto readFloat4 = [&](const unsigned char* src) -> float {
            float v; std::memcpy(&v, src, 4);
            if (needSwap) {
                unsigned char* b = reinterpret_cast<unsigned char*>(&v);
                std::swap(b[0], b[3]); std::swap(b[1], b[2]);
            }
            return v;
        };
        auto readU8fromProp = [&](const unsigned char* pt, int pi) -> uint8_t {
            const unsigned char* src = pt + offsets[pi];
            if (propSizes[pi] == 1) return *src;
            if (propSizes[pi] == 4) return static_cast<uint8_t>(readFloat4(src));
            return 0;
        };

        for (int n = 0; n < numVertices; ++n) {
            const unsigned char* pt = base + n * stride;
            result.points.push_back(readFloat4(pt + offsets[xi]));
            result.points.push_back(readFloat4(pt + offsets[yi]));
            result.points.push_back(readFloat4(pt + offsets[zi]));
            if (hasColor) {
                result.colors.push_back(readU8fromProp(pt, ri));
                result.colors.push_back(readU8fromProp(pt, gi));
                result.colors.push_back(readU8fromProp(pt, bi));
                if (hasAlpha) result.colors.push_back(readU8fromProp(pt, ai));
            }
        }
    } else {
        throw std::runtime_error("PLY format not supported: " + plyFormat);
    }
    return result;
}

// ── Save helpers ──────────────────────────────────────────────────────────────

static void savePCD(const std::vector<float>& points,
                    const std::vector<uint8_t>& colors, bool hasAlpha,
                    const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Failed to open file for writing: " + path);
    int numPoints = (int)points.size() / 3;
    bool hasColor = !colors.empty();
    int  colorCh  = hasAlpha ? 4 : 3;

    out << "# .PCD v0.7 - Point Cloud Data file format\n"
        << "VERSION 0.7\n";
    if (hasColor) {
        if (hasAlpha)
            out << "FIELDS x y z rgba\nSIZE 4 4 4 4\nTYPE F F F U\nCOUNT 1 1 1 1\n";
        else
            out << "FIELDS x y z rgb\nSIZE 4 4 4 4\nTYPE F F F U\nCOUNT 1 1 1 1\n";
    } else {
        out << "FIELDS x y z\nSIZE 4 4 4\nTYPE F F F\nCOUNT 1 1 1\n";
    }
    out << "WIDTH "    << numPoints << "\n"
        << "HEIGHT 1\n"
        << "VIEWPOINT 0 0 0 1 0 0 0\n"
        << "POINTS "   << numPoints << "\n"
        << "DATA ascii\n";
    out << std::fixed;
    for (int i = 0; i < numPoints; ++i) {
        out << points[i*3] << " " << points[i*3+1] << " " << points[i*3+2];
        if (hasColor && (int)colors.size() >= (i+1)*colorCh) {
            uint32_t raw;
            if (hasAlpha) {
                float packed = packRGBAtoFloat(colors[i*colorCh+0], colors[i*colorCh+1],
                                               colors[i*colorCh+2], colors[i*colorCh+3]);
                std::memcpy(&raw, &packed, 4);
            } else {
                float packed = packRGBtoFloat(colors[i*colorCh+0], colors[i*colorCh+1],
                                              colors[i*colorCh+2]);
                std::memcpy(&raw, &packed, 4);
            }
            out << " " << raw;
        }
        out << "\n";
    }
    if (!out) throw std::runtime_error("Write failed for file: " + path);
}

static void savePLY(const std::vector<float>& points,
                    const std::vector<uint8_t>& colors, bool hasAlpha,
                    const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Failed to open file for writing: " + path);
    int numPoints = (int)points.size() / 3;
    bool hasColor = !colors.empty();
    int  colorCh  = hasAlpha ? 4 : 3;

    out << "ply\nformat ascii 1.0\n"
        << "element vertex " << numPoints << "\n"
        << "property float x\nproperty float y\nproperty float z\n";
    if (hasColor) {
        out << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
        if (hasAlpha) out << "property uchar alpha\n";
    }
    out << "end_header\n" << std::fixed;
    for (int i = 0; i < numPoints; ++i) {
        out << points[i*3] << " " << points[i*3+1] << " " << points[i*3+2];
        if (hasColor && (int)colors.size() >= (i+1)*colorCh) {
            out << " " << static_cast<int>(colors[i*colorCh+0])
                << " " << static_cast<int>(colors[i*colorCh+1])
                << " " << static_cast<int>(colors[i*colorCh+2]);
            if (hasAlpha)
                out << " " << static_cast<int>(colors[i*colorCh+3]);
        }
        out << "\n";
    }
    if (!out) throw std::runtime_error("Write failed for file: " + path);
}

// ── Public interface ──────────────────────────────────────────────────────────

void Pointcloud::load() {
    if (format == PointcloudFormat::UNKNOWN)
        throw std::runtime_error("Pointcloud format not set before load()");

    const std::vector<unsigned char>& bytes = source->getRawBytes();
    if (bytes.empty())
        throw std::runtime_error("Empty data from source");

    PointcloudData d;
    switch (format) {
        case PointcloudFormat::PCD: d = parsePCD(bytes); break;
        case PointcloudFormat::PLY: d = parsePLY(bytes); break;
        default: throw std::runtime_error("Unsupported pointcloud format");
    }
    points    = std::move(d.points);
    colors    = std::move(d.colors);
    hasAlpha_ = d.hasAlpha;
    loaded_   = true;
}

void Pointcloud::saveToFile(const std::string& path, PointcloudFormat fmt) const {
    requireLoaded("Pointcloud");
    switch (fmt) {
        case PointcloudFormat::PCD: savePCD(points, colors, hasAlpha_, path); break;
        case PointcloudFormat::PLY: savePLY(points, colors, hasAlpha_, path); break;
        default: throw std::runtime_error("Unsupported pointcloud format for saving");
    }
}

void Pointcloud::saveToFile(const std::string& path) const {
    saveToFile(path, format);
}
