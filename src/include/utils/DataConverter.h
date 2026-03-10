#pragma once
#include <memory>
#include <string>
#include <vector>
#include "datatype/Image.h"
#include "datatype/Text.h"
#include "datatype/Video.h"

class DataConverter {
public:
    static std::shared_ptr<Image> convertImageFormat(
        const std::shared_ptr<Image>& img,
        const std::string& targetFormat
    );

    static std::shared_ptr<Video> imagesToVideo(
        const std::vector<std::shared_ptr<Image>>& images,
        double fps
    );

    static std::vector<std::shared_ptr<Image>> videoToImages(
        const std::shared_ptr<Video>& video
    );

};