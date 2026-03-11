#pragma once
#include <memory>
#include <string>
#include <vector>
#include "datatype/Image.h"
#include "datatype/Text.h"
#include "datatype/Video.h"

class DataInfo {
public:
    //Image
    static void printImageInfo(const std::shared_ptr<Image>& img, const std::string& type);
    static void displayImage(const std::shared_ptr<Image>& img, const std::string& type);
    
    //Video
    static void printVideoInfo(const std::shared_ptr<Video>& video);

    //Text
    static void printTextInfo(const std::shared_ptr<Text>& text);
};