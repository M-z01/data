#pragma once
#include <memory>
#include <string>
#include <vector>
#include "datatype/Image.h"
#include "datatype/Text.h"
#include "datatype/Video.h"
#include "datatype/Pointcloud.h"

enum class ImageViewType {
    GENERIC,
    MASK,
    DEPTH
};

class DataInfo {
public:
    //Image
    static void printImageInfo(const std::shared_ptr<Image>& img, ImageViewType type = ImageViewType::GENERIC);
    static void displayImage(const std::shared_ptr<Image>& img, ImageViewType type = ImageViewType::GENERIC);
    
    //Video
    static void printVideoInfo(const std::shared_ptr<Video>& video);

    //Text
    static void printTextInfo(const std::shared_ptr<Text>& text);
    static void projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Image>& img);

    //Pointcloud
    static void printPointsInfo(const std::shared_ptr<Pointcloud>& pc);
    static void projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Pointcloud>& pc);
};