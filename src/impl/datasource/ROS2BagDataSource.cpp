#include "datasource/ROS2BagDataSource.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// ROS2 / rosbag2 headers (requires sourced ROS2 environment)
#include <rclcpp/rclcpp.hpp>
#include <rosbag2_cpp/reader.hpp>
#include <rosbag2_storage/storage_options.hpp>
#include <rosbag2_cpp/converter_options.hpp>

// For image decoding you need OpenCV (mirrors Pillow in Python)
#include <opencv2/opencv.hpp>

// nlohmann/json for writing index.json (or use your own JSON util)
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ── helpers ──────────────────────────────────────────────────────────────────

std::string ROS2BagDataSource::sanitizeTopicName(const std::string& topic) const {
    // mirrors sanitize_topic_name(): strip leading '/', replace '/' with '_'
    std::string s = topic;
    if (!s.empty() && s[0] == '/') s.erase(0, 1);
    std::replace(s.begin(), s.end(), '/', '_');
    return s;
}

bool ROS2BagDataSource::isImageTopic(const std::string& msgtype) const {
    // mirrors is_image_topic()
    return msgtype.find("Image") != std::string::npos;
}

// ── constructor ───────────────────────────────────────────────────────────────

ROS2BagDataSource::ROS2BagDataSource(const std::string& bagPath,
                                     const std::string& outputDir)
    : bagPath(bagPath), outputDir(outputDir)
{
    if (!fs::exists(bagPath))
        throw std::runtime_error("Bag path not found: " + bagPath);
}

// ── getAvailableTopics ────────────────────────────────────────────────────────

std::vector<TopicInfo> ROS2BagDataSource::getAvailableTopics() const {
    // mirrors get_available_topics()
    rosbag2_cpp::Reader reader;
    rosbag2_storage::StorageOptions storageOpts;
    storageOpts.uri        = bagPath;
    storageOpts.storage_id = "sqlite3";

    rosbag2_cpp::ConverterOptions convOpts;
    convOpts.input_serialization_format  = "cdr";
    convOpts.output_serialization_format = "cdr";

    reader.open(storageOpts, convOpts);

    std::vector<TopicInfo> result;
    for (auto& meta : reader.get_all_topics_and_types())
        result.push_back({meta.name, meta.type});
    return result;
}

// ── extractTopics ─────────────────────────────────────────────────────────────

void ROS2BagDataSource::extractTopics(const std::vector<std::string>& topics) {
    // mirrors read_bag()

    fs::create_directories(outputDir);

    // Build topic -> msgtype map from the bag
    auto available = getAvailableTopics();
    std::unordered_map<std::string, std::string> topicTypeMap;
    for (auto& t : available)
        topicTypeMap[t.topic] = t.msgtype;

    // Decide which topics to process
    std::vector<std::string> selected = topics;
    if (selected.empty())
        for (auto& t : available)
            selected.push_back(t.topic);

    // Validate and create per-topic output dirs
    std::unordered_map<std::string, std::string>              topicDirs;
    std::unordered_map<std::string, std::vector<MessageEntry>> topicIndices;

    for (auto& topic : selected) {
        if (topicTypeMap.find(topic) == topicTypeMap.end()) {
            std::cerr << "[WARN] Topic '" << topic << "' not found in bag. Skipping.\n";
            continue;
        }
        std::string dir = outputDir + "/" + sanitizeTopicName(topic);
        fs::create_directories(dir);
        topicDirs[topic]    = dir;
        topicIndices[topic] = {};
        std::cout << "[INFO] Output for '" << topic << "' -> " << dir << "\n";
    }

    if (topicDirs.empty()) {
        std::cerr << "[ERROR] No valid topics selected.\n";
        return;
    }

    // Open reader with topic filter
    rosbag2_cpp::Reader reader;
    rosbag2_storage::StorageOptions storageOpts;
    storageOpts.uri        = bagPath;
    storageOpts.storage_id = "sqlite3";

    rosbag2_cpp::ConverterOptions convOpts;
    convOpts.input_serialization_format  = "cdr";
    convOpts.output_serialization_format = "cdr";

    reader.open(storageOpts, convOpts);

    rosbag2_storage::StorageFilter filter;
    for (auto& [t, _] : topicDirs)
        filter.topics.push_back(t);
    reader.set_filter(filter);

    // Read messages
    while (reader.has_next()) {
        auto bag_msg = reader.read_next();
        const std::string& topic = bag_msg->topic_name;

        if (topicDirs.find(topic) == topicDirs.end()) continue;

        uint64_t ts_ns  = static_cast<uint64_t>(bag_msg->time_stamp);
        double   ts_sec = ts_ns / 1e9;

        // Build timestamp string: YYYYMMDD_HHMMSS_ffffff  (mirrors dt_str in Python)
        std::time_t t = static_cast<std::time_t>(ts_sec);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::gmtime(&t));
        uint64_t micros = (ts_ns % 1'000'000'000ULL) / 1000ULL;
        std::ostringstream dt_str;
        dt_str << buf << "_" << std::setw(6) << std::setfill('0') << micros;

        const std::string& msgtype  = topicTypeMap[topic];
        const std::string& dir      = topicDirs[topic];
        const auto&        raw      = bag_msg->serialized_data;

        if (isImageTopic(msgtype)) {
            // Deserialize sensor_msgs/Image via rclcpp
            // For a generic approach we use the raw CDR bytes with OpenCV
            // (full deserialization requires rosidl — simpler to call
            //  rclcpp::SerializedMessage helpers here)
            try {
                rclcpp::SerializedMessage serialized(*raw);
                auto img_msg = std::make_shared<sensor_msgs::msg::Image>();
                rclcpp::Serialization<sensor_msgs::msg::Image> serializer;
                serializer.deserialize_message(&serialized, img_msg.get());

                // Convert to OpenCV mat (mirrors decode_image())
                cv::Mat frame;
                if (img_msg->encoding == "rgb8") {
                    frame = cv::Mat(img_msg->height, img_msg->width, CV_8UC3,
                                    const_cast<uint8_t*>(img_msg->data.data()));
                    cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
                } else if (img_msg->encoding == "bgr8") {
                    frame = cv::Mat(img_msg->height, img_msg->width, CV_8UC3,
                                    const_cast<uint8_t*>(img_msg->data.data()));
                } else if (img_msg->encoding == "mono8") {
                    frame = cv::Mat(img_msg->height, img_msg->width, CV_8UC1,
                                    const_cast<uint8_t*>(img_msg->data.data()));
                } else if (img_msg->encoding == "mono16" || img_msg->encoding == "16UC1") {
                    frame = cv::Mat(img_msg->height, img_msg->width, CV_16UC1,
                                    const_cast<uint8_t*>(img_msg->data.data()));
                } else {
                    throw std::runtime_error("Unsupported encoding: " + img_msg->encoding);
                }

                std::string filename = dt_str.str() + ".png";
                cv::imwrite(dir + "/" + filename, frame);
                topicIndices[topic].push_back({ts_ns, ts_sec, filename});

            } catch (const std::exception& e) {
                std::cerr << "[WARN] Failed to decode image at " << ts_ns << ": " << e.what() << "\n";
            }

        } else {
            // Non-image: save raw CDR bytes as .bin (JSON serialization requires
            // rosidl introspection — store raw for now, or extend with your text types)
            try {
                std::string filename = dt_str.str() + ".bin";
                std::ofstream ofs(dir + "/" + filename, std::ios::binary);
                ofs.write(reinterpret_cast<const char*>(raw->buffer), raw->buffer_length);

                topicIndices[topic].push_back({ts_ns, ts_sec, filename});

            } catch (const std::exception& e) {
                std::cerr << "[WARN] Failed to save message at " << ts_ns << ": " << e.what() << "\n";
            }
        }
    }

    // Write index.json per topic  (mirrors the Python index writing)
    for (auto& [topic, entries] : topicIndices) {
        json index;
        index["topic"]          = topic;
        index["msgtype"]        = topicTypeMap[topic];
        index["total_messages"] = entries.size();

        json msgs = json::array();
        for (auto& e : entries) {
            msgs.push_back({
                {"timestamp_ns",  e.timestamp_ns},
                {"timestamp_sec", e.timestamp_sec},
                {"file",          e.file}
            });
        }
        index["messages"] = msgs;

        std::string indexPath = topicDirs[topic] + "/index.json";
        std::ofstream ofs(indexPath);
        ofs << index.dump(2);
        std::cout << "[INFO] '" << topic << "': " << entries.size()
                  << " messages saved. Index -> " << indexPath << "\n";
    }
}

// ── getRawBytes ───────────────────────────────────────────────────────────────

std::vector<unsigned char> ROS2BagDataSource::getRawBytes() {
    // Return the combined index.json bytes for all extracted topics
    // (satisfies the DataSource interface)
    json combined = json::array();

    for (auto& entry : fs::directory_iterator(outputDir)) {
        if (!entry.is_directory()) continue;
        fs::path indexPath = entry.path() / "index.json";
        if (!fs::exists(indexPath)) continue;

        std::ifstream ifs(indexPath);
        json idx;
        ifs >> idx;
        combined.push_back(idx);
    }

    std::string s = combined.dump(2);
    return std::vector<unsigned char>(s.begin(), s.end());
}