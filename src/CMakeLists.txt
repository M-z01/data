cmake_minimum_required(VERSION 3.10)
project(DataProcessing CXX)

# Require C++17 (required by rosbag2/rclcpp)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(include)

# Find OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Find ROS2 packages
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosbag2_cpp REQUIRED)
find_package(rosbag2_storage REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nlohmann_json REQUIRED)

# Gather all source files
file(GLOB_RECURSE SOURCES "impl/*.cpp")

# Create executable
add_executable(main runner/main.cpp ${SOURCES})

# Link OpenCV
target_link_libraries(main ${OpenCV_LIBS})

# Link ROS2
ament_target_dependencies(main
    rclcpp
    rosbag2_cpp
    rosbag2_storage
    sensor_msgs
)

# Link nlohmann_json
target_link_libraries(main nlohmann_json::nlohmann_json)

ament_package()
