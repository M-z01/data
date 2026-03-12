// FormatDetector.h — centralised, case-insensitive extension → format helpers.
// Replace every per-factory resolveXFormat() and the duplicate helpers in main.cpp
// with calls into this namespace.
#pragma once
#include <string>
#include <algorithm>
#include <filesystem>

namespace FormatDetector {

/// Return the lowercase file extension including the leading dot (e.g. ".jpg").
inline std::string lowerExt(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

/// Returns "JPG", "PNG", "EXR", or "" if the extension is not recognised.
inline std::string imageFormat(const std::string& path) {
    const std::string ext = lowerExt(path);
    if (ext == ".jpg" || ext == ".jpeg") return "JPG";
    if (ext == ".png")                   return "PNG";
    if (ext == ".exr")                   return "EXR";
    return "";
}

/// Returns "MP4", "AVI", "MKV", or "" if the extension is not recognised.
inline std::string videoFormat(const std::string& path) {
    const std::string ext = lowerExt(path);
    if (ext == ".mp4" || ext == ".mov" || ext == ".m4v") return "MP4";
    if (ext == ".avi")                                    return "AVI";
    if (ext == ".mkv" || ext == ".webm")                  return "MKV";
    return "";
}

/// Returns "TXT", "CSV", "JSON", or "" if the extension is not recognised.
inline std::string textFormat(const std::string& path) {
    const std::string ext = lowerExt(path);
    if (ext == ".txt")  return "TXT";
    if (ext == ".csv")  return "CSV";
    if (ext == ".json") return "JSON";
    return "";
}

/// Returns "PCD", "PLY", or "" if the extension is not recognised.
inline std::string pointcloudFormat(const std::string& path) {
    const std::string ext = lowerExt(path);
    if (ext == ".pcd") return "PCD";
    if (ext == ".ply") return "PLY";
    return "";
}

/// Returns the broad data type: "image", "video", "text", "pointcloud", or "".
inline std::string detectType(const std::string& path) {
    if (!imageFormat(path).empty())      return "image";
    if (!videoFormat(path).empty())      return "video";
    if (!textFormat(path).empty())       return "text";
    if (!pointcloudFormat(path).empty()) return "pointcloud";
    return "";
}

} // namespace FormatDetector
