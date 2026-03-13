#include <iostream>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>

#include <opencv2/opencv.hpp>
#include "utils/OpenCVBridge.h"
#include "utils/FormatDetector.h"
#include "loaders/ImageFactory.h"
#include "loaders/PointcloudFactory.h"
#include "loaders/TextFactory.h"
#include "loaders/VideoFactory.h"
#include "utils/DataInfo.h"
#include "utils/DataVisualize.h"
#include "utils/DataConverter.h"

namespace fs = std::filesystem;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
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
    DataVisualize::displayImage(img, viewType);

    if (!dst.empty()) {
        const std::string srcFmt = FormatDetector::imageFormat(src.string());
        const std::string dstFmt = FormatDetector::imageFormat(dst.string());
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
        const std::string srcFmt = FormatDetector::textFormat(src.string());
        const std::string dstFmt = FormatDetector::textFormat(dst.string());
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
    // Build Image objects without loading pixel data yet.
    // encodeImagesToFile loads each image just before encoding its frame
    // and unloads it immediately after — peak RAM is one decoded image.
    std::vector<std::shared_ptr<Image>> images;
    images.reserve(imgPaths.size());
    for (const auto& p : imgPaths) {
        const std::string fmt = FormatDetector::imageFormat(p.string());
        if (fmt.empty()) throw std::runtime_error("Unsupported image extension: " + p.string());
        images.push_back(ImageFactory::createImage(ImageSourceType::FILE, p.string()));
    }

    std::cout << "[ImagesToVideo] Encoding " << images.size()
              << " frame(s) at " << fps << " fps -> " << dst << "\n";
    DataConverter::encodeImagesToFile(images, fps, dst.string());
    std::cout << "[ImagesToVideo] Saved to: " << dst << "\n";
}

static std::vector<fs::path> collectImagesFromDir(const fs::path& dir) {
    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (!FormatDetector::imageFormat(entry.path().string()).empty())
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
    // scanMetadata() opens the source and reads fps/dimensions from the first
    // frame without storing any pixel data — keeps RAM usage near zero.
    video->scanMetadata();
    DataInfo::printVideoInfo(video);

    fs::create_directories(outDir);

    // Stream decode: only one frame's pixel data is in RAM at a time.
    // Each frame is PNG-encoded and saved to disk before the next is decoded.
    std::size_t saved = 0;
    video->forEachFrame([&](std::size_t i, const ImageBuffer& buf) {
        char fname[64];
        std::snprintf(fname, sizeof(fname), "frame_%04zu.png", i);
        const fs::path outPath = outDir / fname;

        cv::Mat mat = OpenCVBridge::bufferToMat(buf);
        if (!cv::imwrite(outPath.string(), mat))
            throw std::runtime_error("[VideoToImages] cv::imwrite failed: " + outPath.string());

        ++saved;
        if (i == 0 || (i + 1) % 10 == 0)
            std::cout << "[VideoToImages] Saved " << outPath << "\n";
    });
    std::cout << "[VideoToImages] Done — saved " << saved << " frame(s) to " << outDir << "\n";
}

static void testPointcloud(const fs::path& src, const fs::path& dst) {
    std::cout << "[Pointcloud] Loading: " << src << "\n";
    auto pc = PointcloudFactory::createPointcloud(PointcloudSourceType::FILE, src.string());
    pc->load();
    DataInfo::printPointsInfo(pc);
    DataVisualize::displayPointcloud(pc);

    if (!dst.empty()) {
        const std::string srcFmt = FormatDetector::pointcloudFormat(src.string());
        const std::string dstFmt = FormatDetector::pointcloudFormat(dst.string());
        if (!dstFmt.empty() && dstFmt != srcFmt) {
            std::cout << "[Pointcloud] Converting " << srcFmt << " -> " << dstFmt << "\n";
            auto converted = DataConverter::convertPointcloudFormat(pc, dstFmt);
            converted->saveToFile(dst.string());
        } else {
            pc->saveToFile(dst.string());
        }
        std::cout << "[Pointcloud] Saved to: " << dst << "\n";
    }
}

static void testVideo(const fs::path& src, const fs::path& dst) {
    std::cout << "[Video] Loading: " << src << "\n";
    auto video = VideoFactory::createVideo(VideoSourceType::FILE, src.string());
    video->load();
    DataInfo::printVideoInfo(video);

    if (!dst.empty()) {
        const std::string srcFmt = FormatDetector::videoFormat(src.string());
        const std::string dstFmt = FormatDetector::videoFormat(dst.string());
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

// project-bbox: project 3D bounding boxes from metadata onto images or point clouds
// Usage (single):  main project-bbox <meta.json> <image_or_pointcloud> [output_path]
// Usage (batch):   main project-bbox <base_dir>  (expects color/, meta/, points/ subdirs)
static void testProjectBBox(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " project-bbox <meta.json> <image.png|ply> [output_path]\n"
                  << "  " << argv[0] << " project-bbox <base_dir>\n"
                  << "    base_dir must contain color/, meta/, and optionally points/ subdirs\n";
        std::exit(1);
    }

    const fs::path arg2 = argv[2];

    // ── Batch mode: base_dir with color/ + meta/ (+ optional points/) ────────
    if (fs::is_directory(arg2)) {
        const fs::path colorDir  = arg2 / "color";
        const fs::path metaDir   = arg2 / "meta";
        const fs::path pointsDir = arg2 / "points";

        if (!fs::is_directory(metaDir)) {
            std::cerr << "Error: meta/ subdirectory not found in " << arg2 << "\n";
            std::exit(1);
        }

        // Determine whether to project onto images or point clouds
        bool hasColor  = fs::is_directory(colorDir);
        bool hasPoints = fs::is_directory(pointsDir);

        if (!hasColor && !hasPoints) {
            std::cerr << "Error: Need color/ or points/ subdirectory in " << arg2 << "\n";
            std::exit(1);
        }

        // Collect meta files (only numeric frame filenames)
        std::vector<std::string> frameIds;
        for (auto& entry : fs::directory_iterator(metaDir)) {
            if (!entry.is_regular_file()) continue;
            std::string stem = entry.path().stem().string();
            bool allDigits = !stem.empty() && std::all_of(stem.begin(), stem.end(), ::isdigit);
            if (allDigits && entry.path().extension() == ".json")
                frameIds.push_back(stem);
        }
        std::sort(frameIds.begin(), frameIds.end());
        std::cout << "Found " << frameIds.size() << " metadata frames\n";

        if (hasColor) {
            fs::path outDir = arg2 / "bbox_projections";
            fs::create_directories(outDir);
            int ok = 0, fail = 0;
            for (const auto& fid : frameIds) {
                fs::path imgPath  = colorDir / (fid + ".png");
                fs::path metaPath = metaDir  / (fid + ".json");
                if (!fs::exists(imgPath))  { ++fail; continue; }
                if (!fs::exists(metaPath)) { ++fail; continue; }

                auto text = TextFactory::createText(TextSourceType::FILE, metaPath.string());
                text->load();
                auto img = ImageFactory::createImage(ImageSourceType::FILE, imgPath.string());
                img->load();
                DataVisualize::projectTextContent(text, img);
                // Save (projectTextContent already stored the drawn buffer in img)
                img->saveToFile((outDir / (fid + ".png")).string());
                ++ok;
            }
            std::cout << "Image projection complete: " << ok << " ok, " << fail << " failed\n"
                      << "Results saved to " << outDir << "\n";
        }

        if (hasPoints) {
            fs::path outDir = arg2 / "bbox_pointclouds";
            fs::create_directories(outDir);
            int ok = 0, fail = 0;
            for (const auto& fid : frameIds) {
                fs::path plyPath  = pointsDir / (fid + ".ply");
                fs::path metaPath = metaDir   / (fid + ".json");
                if (!fs::exists(plyPath))  { ++fail; continue; }
                if (!fs::exists(metaPath)) { ++fail; continue; }

                auto text = TextFactory::createText(TextSourceType::FILE, metaPath.string());
                text->load();
                auto pc = PointcloudFactory::createPointcloud(PointcloudSourceType::FILE, plyPath.string());
                pc->load();
                DataVisualize::projectTextContent(text, pc);
                pc->saveToFile((outDir / (fid + ".ply")).string());
                ++ok;
            }
            std::cout << "Pointcloud projection complete: " << ok << " ok, " << fail << " failed\n"
                      << "Results saved to " << outDir << "\n";
        }
        return;
    }

    // ── Single-file mode: project-bbox <meta.json> <image_or_pointcloud> [output] ──
    const fs::path metaPath  = argv[2];
    const fs::path dataPath  = argv[3];
    const fs::path outputPath = (argc >= 5) ? fs::path(argv[4]) : fs::path{};

    if (!fs::exists(metaPath) || !fs::exists(dataPath)) {
        std::cerr << "Error: file(s) not found\n";
        std::exit(1);
    }

    auto text = TextFactory::createText(TextSourceType::FILE, metaPath.string());
    text->load();

    const std::string dataType = FormatDetector::detectType(dataPath.string());
    if (dataType == "image") {
        auto img = ImageFactory::createImage(ImageSourceType::FILE, dataPath.string());
        img->load();
        DataVisualize::projectTextContent(text, img);
        if (!outputPath.empty()) {
            img->saveToFile(outputPath.string());
            std::cout << "[ProjectBBox] Saved to " << outputPath << "\n";
        }
    } else if (dataType == "pointcloud") {
        auto pc = PointcloudFactory::createPointcloud(PointcloudSourceType::FILE, dataPath.string());
        pc->load();
        DataVisualize::projectTextContent(text, pc);
        if (!outputPath.empty()) {
            pc->saveToFile(outputPath.string());
            std::cout << "[ProjectBBox] Saved to " << outputPath << "\n";
        }
    } else {
        std::cerr << "Error: unsupported data type for projection: " << dataPath.extension() << "\n";
        std::exit(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <input_path> [output_path]\n"
                  << "      Supported types: jpg, jpeg, png, exr, txt, csv, json, mp4, avi, mkv, pcd, ply\n"
                  << "  " << argv[0] << " images-to-video  <input_dir> <output.mp4|avi|mkv> [fps=30]\n"
                  << "  " << argv[0] << " images-to-video  <output.mp4|avi|mkv> <fps> <img1> [img2 ...]\n"
                  << "  " << argv[0] << " video-to-images  <input_video> <output_dir>\n"
                  << "  " << argv[0] << " project-bbox     <meta.json> <image|ply> [output]\n"
                  << "  " << argv[0] << " project-bbox     <base_dir>\n";
        return 1;
    }

    const std::string mode = argv[1];

    try {
        if (mode == "project-bbox" || mode == "project_bbox") {
            testProjectBBox(argc, argv);
            return 0;
        }

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

        const std::string type = FormatDetector::detectType(src.string());
        if (type == "image") {
            testImage(src, dst);
        } else if (type == "text") {
            testText(src, dst);
        } else if (type == "video") {
            testVideo(src, dst);
        } else if (type == "pointcloud") {
            testPointcloud(src, dst);
        } else {
            std::cerr << "Unsupported file extension: " << src.extension() << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
