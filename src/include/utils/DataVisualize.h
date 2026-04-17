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
    static void displayImage(const std::shared_ptr<Image>& img, ImageViewType type = ImageViewType::GENERIC,
                             const std::shared_ptr<Image>& mask = nullptr,
                             bool denormMask = false);

    //Pointcloud
    static void displayPointcloud(const std::shared_ptr<Pointcloud>& pc,
                                  const std::shared_ptr<Image>& mask = nullptr,
                                  const std::shared_ptr<Image>& rgb = nullptr,
                                  const std::shared_ptr<Image>& depth = nullptr);

    //Projection
    static void projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Image>& img);
    static void projectTextContent(const std::shared_ptr<Text>& text, const std::shared_ptr<Pointcloud>& pc);

};
