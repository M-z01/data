cmake_minimum_required(VERSION 3.10)
project(DataProcessing CXX)

# Require C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(include)

# Find OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Find ROS2
# find_package(rosbag2 REQUIRED)
# include_directories(${rosbag2_INCLUDE_DIRS})

# Gather all source files
file(GLOB_RECURSE SOURCES "impl/*.cpp")

# Create executable
add_executable(main runner/main.cpp ${SOURCES})

# Link libraries
target_link_libraries(main ${OpenCV_LIBS})
# target_link_libraries(main rosbag2 ...)  # link ROS2 libs
