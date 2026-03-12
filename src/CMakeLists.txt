cmake_minimum_required(VERSION 3.10)
project(DataProcessing CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release (-O3) when the user doesn't specify a build type.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g")

# Find dependencies
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libswscale
)

# Explicit source list — CMake picks up new files automatically on re-configure
# without the glob re-scan problem that file(GLOB_RECURSE ...) has.
set(SOURCES
    impl/datasource/FileDataSource.cpp
    impl/datatype/Image.cpp
    impl/datatype/Pointcloud.cpp
    impl/datatype/Text.cpp
    impl/datatype/Video.cpp
    impl/loaders/ImageFactory.cpp
    impl/loaders/PointcloudFactory.cpp
    impl/loaders/TextFactory.cpp
    impl/loaders/VideoFactory.cpp
    impl/utils/DataConverter.cpp
    impl/utils/DataInfo.cpp
)

add_executable(main runner/main.cpp ${SOURCES})

# Only expose the public include/ directory; impl/ is internal and should not
# be on the include path for external consumers.
target_include_directories(main PRIVATE
    include
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(main PRIVATE
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
)