#pragma once
#include <memory>
#include <string>
#include <vector>
#include "datatype/Image.h"
#include "datatype/Text.h"
#include "datatype/Video.h"
#include "datatype/Pointcloud.h"

class DataConverter {
public:
    static std::shared_ptr<Image> convertImageFormat(
        const std::shared_ptr<Image>& img,
        const std::string& targetFormat
    );

    static std::shared_ptr<Video> convertVideoFormat(
        const std::shared_ptr<Video>& video,
        const std::string& targetFormat,
        double targetFps = -1.0
    );

    static std::shared_ptr<Video> imagesToVideo(
        const std::vector<std::shared_ptr<Image>>& images,
        double fps,
        const std::string& targetFormat = "MP4"
    );

    // Encodes images directly to an output video file without buffering
    // the encoded data in memory. Prefer this over imagesToVideo() when
    // you only need the file and not an in-memory Video object.
    // Output format is inferred from the file extension (.mp4 / .avi / .mkv).
    static void encodeImagesToFile(
        const std::vector<std::shared_ptr<Image>>& images,
        double fps,
        const std::string& outputPath
    );

    static std::vector<std::shared_ptr<Image>> videoToImages(
        const std::shared_ptr<Video>& video
    );

    static std::shared_ptr<Text> convertTextFormat(
        const std::shared_ptr<Text>& text,
        const std::string& targetFormat
    );

    static std::shared_ptr<Pointcloud> convertPointcloudFormat(
        const std::shared_ptr<Pointcloud>& pc,
        const std::string& targetFormat
    );

    // RGBD -> Pointcloud or Stereo -> Pointcloud conversion
    static std::shared_ptr<Pointcloud> imagesToPointcloud(
        const std::vector<std::shared_ptr<Image>>& rgbs,    // RGB images
        const std::vector<std::shared_ptr<Image>>& depths,  // depth maps aligned with RGBs
        const std::vector<std::shared_ptr<Image>>& segs     // masks for segmentation, optional
    );
    
};