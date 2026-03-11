#include <iostream>
#include <memory>
#include <string>

#include "loaders/VideoFactory.h"
#include "utils/DataConverter.h"

int main() {
    std::string inputFilePath = "/home/kewei/桌面/paper_video/1（复件）.mp4";
    std::string targetFormat = "AVI";

    std::cout << "Loading video from: " << inputFilePath << std::endl;
    
    // Create the video using VideoFactory
    auto video = VideoFactory::createVideo(VideoSourceType::FILE, inputFilePath);
    if (!video) {
        std::cerr << "Failed to create video object." << std::endl;
        return 1;
    }

    // Load the video data
    video->load();
    std::cout << "Video loaded successfully. Current format: MP4" << std::endl;

    std::cout << "Converting video to format: " << targetFormat << std::endl;
    // Convert the video format using DataConverter
    auto convertedVideo = DataConverter::convertVideoFormat(video, targetFormat);
    if (!convertedVideo) {
        std::cerr << "Failed to convert video." << std::endl;
        return 1;
    }

    std::string outputPath = "output.avi";
    std::cout << "Saving converted video to: " << outputPath << std::endl;
    convertedVideo->saveToFile(outputPath);

    std::cout << "Successfully converted mp4 to avi." << std::endl;

    return 0;
}
