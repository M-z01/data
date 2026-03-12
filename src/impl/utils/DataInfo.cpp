#include "utils/DataInfo.h"
#include "utils/OpenCVBridge.h"
#include <iostream>
#include <opencv2/opencv.hpp>

using namespace OpenCVBridge;

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

//Pointcloud
void DataInfo::printPointsInfo(const std::shared_ptr<Pointcloud>& pc) {
    const std::vector<float>& pts = pc->getPoints();
    int numPoints = (int)pts.size() / 3;

    std::cout << "=== Pointcloud Info ===\n";
    std::cout << "  Format    : " << toString(pc->getFormat()) << "\n";
    std::cout << "  Points    : " << numPoints << "\n";

    if (numPoints > 0) {
        float minX = pts[0], maxX = pts[0];
        float minY = pts[1], maxY = pts[1];
        float minZ = pts[2], maxZ = pts[2];
        for (int i = 0; i < numPoints; ++i) {
            float x = pts[i*3], y = pts[i*3+1], z = pts[i*3+2];
            if (x < minX) minX = x; if (x > maxX) maxX = x;
            if (y < minY) minY = y; if (y > maxY) maxY = y;
            if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
        }
        std::cout << "  X range   : [" << minX << ", " << maxX << "]\n";
        std::cout << "  Y range   : [" << minY << ", " << maxY << "]\n";
        std::cout << "  Z range   : [" << minZ << ", " << maxZ << "]\n";
    }

    if (pc->hasColors()) {
        const std::vector<uint8_t>& colors = pc->getColors();
        int ch = pc->hasAlpha() ? 4 : 3;
        std::cout << "  Has color : yes (" << (pc->hasAlpha() ? "RGBA" : "RGB") << ")\n";
        if (numPoints > 0) {
            int minR = 255, maxR = 0;
            int minG = 255, maxG = 0;
            int minB = 255, maxB = 0;
            for (int i = 0; i < numPoints && (i + 1) * ch <= (int)colors.size(); ++i) {
                int r = colors[i*ch+0], g = colors[i*ch+1], b = colors[i*ch+2];
                if (r < minR) minR = r; if (r > maxR) maxR = r;
                if (g < minG) minG = g; if (g > maxG) maxG = g;
                if (b < minB) minB = b; if (b > maxB) maxB = b;
            }
            std::cout << "  R range   : [" << minR << ", " << maxR << "]\n";
            std::cout << "  G range   : [" << minG << ", " << maxG << "]\n";
            std::cout << "  B range   : [" << minB << ", " << maxB << "]\n";
        }
    } else {
        std::cout << "  Has color : no\n";
    }
    std::cout << "=======================\n";
}