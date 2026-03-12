#pragma once
#include <memory>
#include <string>
#include <vector>
#include "datatype/Image.h"
#include "datatype/Text.h"
#include "datatype/Video.h"
#include "datatype/Pointcloud.h"
#include "datatype/ImageViewType.h"

class DataVisualize {
public:
    //Image
    static void displayImage(const std::shared_ptr<Image>& img, ImageViewType type = ImageViewType::GENERIC);

    //Pointcloud
    static void displayPointcloud(const std::shared_ptr<Pointcloud>& pc);

    //Projection
    static void projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Image>& img);
    static void projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Pointcloud>& pc);

};
