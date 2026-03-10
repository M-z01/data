#include "datatype/Video.h"
#include <stdexcept>
#include <fstream>
#include <opencv2/opencv.hpp>

void Video::load() {
    std::vector<unsigned char> bytes = source->getRawBytes();
    if (bytes.empty()) {
        throw std::runtime_error("Empty data from source");
    }

    //Write bytes to a temporary file
    std::string tempPath = "temp_video_buffer.tmp";
    std::ofstream tempFile(tempPath, std::ios::binary);
    tempFile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    tempFile.close();

    // Open with VideoCapture
    cv::VideoCapture cap(tempPath);
    if (!cap.isOpened()) {
        throw std::runtime_error("Failed to open video stream from bytes");
    }

    // Extract frames
    cv::Mat frame;
    while (cap.read(frame)) {
        frames.push_back(frame.clone()); // Clone is necessary as read() reuses the buffer
    }

    cap.release();
    std::remove(tempPath.c_str()); // Clean up
}