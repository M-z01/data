#pragma once
#include <memory>
#include <string>
#include <vector>
#include "datatype/Image.h"
#include "datatype/Text.h"
#include "datatype/Video.h"
#include "datatype/Pointcloud.h"
#include "datatype/ImageViewType.h"

class DataInfo {
public:
    //Image
    static void printImageInfo(const std::shared_ptr<Image>& img, ImageViewType type = ImageViewType::GENERIC, bool denormMask = false);
    
    //Video
    static void printVideoInfo(const std::shared_ptr<Video>& video);

    //Text
    static void printTextInfo(const std::shared_ptr<Text>& text);

    //Pointcloud
    static void printPointsInfo(const std::shared_ptr<Pointcloud>& pc);

};