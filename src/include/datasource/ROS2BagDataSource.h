#pragma once
#include "datasource/DataSource.h"
#include <string>
#include <vector>
#include <functional>

// Forward declare to avoid pulling in ROS2 headers everywhere
struct TopicInfo {
    std::string topic;
    std::string msgtype;
};

struct MessageEntry {
    uint64_t timestamp_ns;
    double   timestamp_sec;
    std::string file;  // relative path under topic dir
};

class ROS2BagDataSource : public DataSource {
public:
    explicit ROS2BagDataSource(const std::string& bagPath, const std::string& outputDir = "./bag_output");

    // Extract all topics (or specified ones) from the bag to outputDir
    // Mirrors read_bag() in the Python script
    void extractTopics(const std::vector<std::string>& topics = {});

    // List available topics in the bag (mirrors get_available_topics())
    std::vector<TopicInfo> getAvailableTopics() const;

    // DataSource interface — returns raw bytes of the index.json for all extracted topics
    std::vector<unsigned char> getRawBytes() override;

    std::string getBagPath()    const { return bagPath; }
    std::string getOutputDir()  const { return outputDir; }

private:
    std::string bagPath;
    std::string outputDir;

    // Internal helpers
    std::string sanitizeTopicName(const std::string& topic) const;
    bool        isImageTopic(const std::string& msgtype) const;
};