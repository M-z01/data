#include <iostream>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>

#include "loaders/ImageFactory.h"
#include "loaders/TextFactory.h"
#include "loaders/VideoFactory.h"
#include "utils/DataInfo.h"
#include "utils/DataConverter.h"

namespace fs = std::filesystem;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string imgExtToFmt(const fs::path& p) {
    const std::string ext = lower(p.extension().string());
    if (ext == ".jpg" || ext == ".jpeg") return "JPG";
    if (ext == ".png")                   return "PNG";
    if (ext == ".exr")                   return "EXR";
    return "";
}

static std::string videoExtToFmt(const fs::path& p) {
    const std::string ext = lower(p.extension().string());
    if (ext == ".mp4") return "MP4";
    if (ext == ".avi") return "AVI";
    if (ext == ".mkv") return "MKV";
    return "";
}

static std::string textExtToFmt(const fs::path& p) {
    const std::string ext = lower(p.extension().string());
    if (ext == ".txt")  return "TXT";
    if (ext == ".csv")  return "CSV";
    if (ext == ".json") return "JSON";
    return "";
}

static ImageViewType inferViewType(const fs::path& src) {
    const std::string stem = lower(src.stem().string());
    if (stem.find("mask") != std::string::npos) return ImageViewType::MASK;
    if (stem.find("depth") != std::string::npos) return ImageViewType::DEPTH;
    return ImageViewType::GENERIC;
}

static void testImage(const fs::path& src, const fs::path& dst) {
    std::cout << "[Image] Loading: " << src << "\n";
    auto img = ImageFactory::createImage(ImageSourceType::FILE, src.string());
    img->load();

    const ImageViewType viewType = inferViewType(src);
    DataInfo::printImageInfo(img, viewType);
    DataInfo::displayImage(img, viewType);

    if (!dst.empty()) {
        const std::string srcFmt = imgExtToFmt(src);
        const std::string dstFmt = imgExtToFmt(dst);
        if (!dstFmt.empty() && dstFmt != srcFmt) {
            std::cout << "[Image] Converting " << srcFmt << " -> " << dstFmt << "\n";
            auto converted = DataConverter::convertImageFormat(img, dstFmt);
            converted->saveToFile(dst.string());
        } else {
            img->saveToFile(dst.string());
        }
        std::cout << "[Image] Saved to: " << dst << "\n";
    }
}

static void testText(const fs::path& src, const fs::path& dst) {
    std::cout << "[Text] Loading: " << src << "\n";
    auto text = TextFactory::createText(TextSourceType::FILE, src.string());
    text->load();
    DataInfo::printTextInfo(text);

    const std::string& content = text->getContent();
    constexpr std::size_t previewLen = 200;
    std::cout << "--- content preview (" << previewLen << " chars) ---\n"
              << content.substr(0, previewLen)
              << (content.size() > previewLen ? "\n[...truncated]" : "")
              << "\n---\n";

    if (!dst.empty()) {
        const std::string srcFmt = textExtToFmt(src);
        const std::string dstFmt = textExtToFmt(dst);
        if (!dstFmt.empty() && dstFmt != srcFmt) {
            std::cout << "[Text] Converting " << srcFmt << " -> " << dstFmt << "\n";
            auto converted = DataConverter::convertTextFormat(text, dstFmt);
            converted->saveToFile(dst.string());
        } else {
            text->saveToFile(dst.string());
        }
        std::cout << "[Text] Saved to: " << dst << "\n";
    }
}

// images-to-video: load N images, encode, save video (format inferred from dst extension)
// Usage (dir):   main images-to-video <input_dir> <output.mp4|avi|mkv> [fps=30]
// Usage (files): main images-to-video <output.mp4|avi|mkv> <fps> <img1> [img2 ...]
static void testImagesToVideo(const fs::path& dst, double fps,
                              const std::vector<fs::path>& imgPaths) {
    const std::string dstFmt = videoExtToFmt(dst);
    if (dstFmt.empty())
        throw std::runtime_error("Unsupported output video extension: " + dst.string()
                                 + ". Supported: .mp4, .avi, .mkv");

    std::vector<std::shared_ptr<Image>> images;
    for (const auto& p : imgPaths) {
        std::cout << "[ImagesToVideo] Loading image: " << p << "\n";
        const std::string fmt = imgExtToFmt(p);
        if (fmt.empty()) throw std::runtime_error("Unsupported image extension: " + p.string());
        auto img = ImageFactory::createImage(ImageSourceType::FILE, p.string());
        img->load();
        DataInfo::printImageInfo(img, inferViewType(p));
        images.push_back(std::move(img));
    }

    std::cout << "[ImagesToVideo] Encoding " << images.size()
              << " frame(s) at " << fps << " fps as " << dstFmt << " -> " << dst << "\n";
    auto video = DataConverter::imagesToVideo(images, fps, dstFmt);
    DataInfo::printVideoInfo(video);
    video->saveToFile(dst.string());
    std::cout << "[ImagesToVideo] Saved to: " << dst << "\n";
}

static std::vector<fs::path> collectImagesFromDir(const fs::path& dir) {
    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (!imgExtToFmt(entry.path()).empty())
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty())
        throw std::runtime_error("No image files found in directory: " + dir.string());
    return paths;
}

// video-to-images: decode video, save each frame as PNG
// Usage: main video-to-images <input.mp4> <output_dir>
static void testVideoToImages(const fs::path& src, const fs::path& outDir) {
    std::cout << "[VideoToImages] Loading: " << src << "\n";
    auto video = VideoFactory::createVideo(VideoSourceType::FILE, src.string());
    video->load();
    DataInfo::printVideoInfo(video);

    fs::create_directories(outDir);
    auto images = DataConverter::videoToImages(video);
    std::cout << "[VideoToImages] Extracted " << images.size() << " frame(s).\n";

    for (std::size_t i = 0; i < images.size(); ++i) {
        char fname[64];
        std::snprintf(fname, sizeof(fname), "frame_%04zu.png", i);
        const fs::path outPath = outDir / fname;
        images[i]->saveToFile(outPath.string());
        if (i == 0 || (i + 1) % 10 == 0 || i + 1 == images.size())
            std::cout << "[VideoToImages] Saved " << outPath << "\n";
    }
}

static void testVideo(const fs::path& src, const fs::path& dst) {
    std::cout << "[Video] Loading: " << src << "\n";
    auto video = VideoFactory::createVideo(VideoSourceType::FILE, src.string());
    video->load();
    DataInfo::printVideoInfo(video);

    if (!dst.empty()) {
        const std::string srcFmt = videoExtToFmt(src);
        const std::string dstFmt = videoExtToFmt(dst);
        if (!dstFmt.empty() && dstFmt != srcFmt) {
            std::cout << "[Video] Converting " << srcFmt << " -> " << dstFmt << "\n";
            auto converted = DataConverter::convertVideoFormat(video, dstFmt);
            converted->saveToFile(dst.string());
        } else {
            video->saveToFile(dst.string());
        }
        std::cout << "[Video] Saved to: " << dst << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <input_path> [output_path]\n"
                  << "      Supported types: jpg, jpeg, png, exr, txt, csv, json, mp4, avi, mkv\n"
                  << "  " << argv[0] << " images-to-video  <input_dir> <output.mp4|avi|mkv> [fps=30]\n"
                  << "  " << argv[0] << " images-to-video  <output.mp4|avi|mkv> <fps> <img1> [img2 ...]\n"
                  << "  " << argv[0] << " video-to-images  <input_video> <output_dir>\n";
        return 1;
    }

    const std::string mode = argv[1];

    try {
        if (mode == "images-to-video" || mode == "images-to-videos") {
            if (argc < 4) {
                std::cerr << "Usage (dir):   " << argv[0]
                          << " images-to-video <input_dir> <output.mp4|avi|mkv> [fps=30]\n"
                          << "Usage (files): " << argv[0]
                          << " images-to-video <output.mp4|avi|mkv> <fps> <img1> [img2 ...]\n";
                return 1;
            }
            const fs::path firstArg = argv[2];
            if (fs::is_directory(firstArg)) {
                // Directory mode: images-to-video <dir> <output> [fps]
                if (argc < 4) {
                    std::cerr << "Usage: " << argv[0]
                              << " images-to-video <input_dir> <output.mp4|avi|mkv> [fps=30]\n";
                    return 1;
                }
                const fs::path dst = argv[3];
                const double   fps = (argc >= 5) ? std::stod(argv[4]) : 30.0;
                testImagesToVideo(dst, fps, collectImagesFromDir(firstArg));
            } else {
                // File-list mode: images-to-video <output> <fps> <img1> [img2 ...]
                if (argc < 5) {
                    std::cerr << "Usage: " << argv[0]
                              << " images-to-video <output.mp4|avi|mkv> <fps> <img1> [img2 ...]\n";
                    return 1;
                }
                const fs::path dst = argv[2];
                const double   fps = std::stod(argv[3]);
                std::vector<fs::path> imgPaths;
                for (int i = 4; i < argc; ++i) imgPaths.emplace_back(argv[i]);
                testImagesToVideo(dst, fps, imgPaths);
            }
            return 0;
        }

        if (mode == "video-to-images" || mode == "video-to-image") {
            if (argc < 4) {
                std::cerr << "Usage: " << argv[0]
                          << " video-to-images <input_video> <output_dir>\n";
                return 1;
            }
            const fs::path src    = argv[2];
            const fs::path outDir = argv[3];
            if (!fs::exists(src)) {
                std::cerr << "Error: file not found: " << src << "\n";
                return 1;
            }
            testVideoToImages(src, outDir);
            return 0;
        }

        // --- original single-file mode ---
        const fs::path src = argv[1];
        const fs::path dst = (argc >= 3) ? fs::path(argv[2]) : fs::path{};

        if (!fs::exists(src)) {
            std::cerr << "Error: file not found: " << src << "\n";
            return 1;
        }

        const std::string ext = lower(src.extension().string());
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".exr") {
            testImage(src, dst);
        } else if (ext == ".txt" || ext == ".csv" || ext == ".json") {
            testText(src, dst);
        } else if (ext == ".mp4" || ext == ".avi" || ext == ".mkv") {
            testVideo(src, dst);
        } else {
            std::cerr << "Unsupported file extension: " << ext << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
