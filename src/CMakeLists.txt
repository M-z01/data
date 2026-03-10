cmake_minimum_required(VERSION 3.10)
project(DataProcessing CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Require C++11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(include)
include_directories(impl)

# Find OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Find FFmpeg via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libswscale
)
include_directories(${FFMPEG_INCLUDE_DIRS})

# Gather all source files
file(GLOB_RECURSE SOURCES "impl/*.cpp")

# Create executable
add_executable(main runner/main.cpp ${SOURCES})

# Test executable
add_executable(test runner/test.cpp ${SOURCES})

# Link libraries
target_link_libraries(main
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
)

target_link_libraries(test
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
)
