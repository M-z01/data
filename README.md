# data

A C++17 toolkit for loading, converting, visualizing, and saving common robotics / vision data types: **images**, **point clouds**, **videos**, and **text / metadata**. Includes a Python ROS 2 bag reader for extracting topics to disk.

---

## Dependencies

| Library | Purpose |
|---------|---------|
| **OpenCV** (with `viz` module) | Image / video I/O, point cloud visualization |
| **FFmpeg** (`libavformat`, `libavcodec`, `libavutil`, `libswscale`) | Video encoding / decoding |
| **nlohmann/json** (≥ 3.2) | JSON metadata parsing |
| **CMake** (≥ 3.10) | Build system |

## Build

```bash
cd src
cmake -B build
cmake --build build -j$(nproc)
```

---

## Usage

All commands are run from the `src/` directory.

### Single-file load / visualize / convert

```bash
# Load, print info, and display
./build/main <input>

# Load, convert format, and save
./build/main <input> <output>
```

Supported types:

| Category | Extensions |
|----------|-----------|
| Image | `.jpg`, `.jpeg`, `.png`, `.exr` |
| Video | `.mp4`, `.avi`, `.mkv` |
| Text | `.txt`, `.csv`, `.json` |
| Point cloud | `.pcd`, `.ply` |

Format conversion happens automatically when input and output extensions differ.

### Segmentation mask overlay

```bash
# Overlay segmentation mask on an image (foreground = non-1.0 pixels)
./build/main <image> --mask <seg.png|exr>

# Optionally provide RGB + depth to back-project from images instead
./build/main <pointcloud> --mask <seg.png|exr> --rgb <rgb.png> --depth <depth.png|exr>
```

### Images → Video

```bash
# From a directory of images (sorted alphabetically)
./build/main images-to-video <input_dir> <output.mp4|avi|mkv> [fps=30]

# From explicit file list
./build/main images-to-video <output.mp4> <fps> <img1> [img2 ...]
```

### Video → Images

```bash
# Decode video frames to PNG
./build/main video-to-images <input_video> <output_dir>
```

### Images → Point Cloud (RGB-D)

```bash
# From directories of aligned RGB + depth images — merge all frames into one file
./build/main images-to-pc <rgb_dir> <depth_dir> <output.pcd|ply> --intrinsics <calib.json> [seg_dir]

# From directories of aligned RGB + depth images — produce one pointcloud per frame
./build/main images-to-pc <rgb_dir> <depth_dir> <output_dir> <pcd|ply> --intrinsics <calib.json> [seg_dir]

# From single files (single RGB + depth -> single pointcloud)
./build/main images-to-pc <rgb.png> <depth.png|exr> <output.pcd|ply> --intrinsics <calib.json> [mask.png|exr]
```

When `output` is a directory you must explicitly provide the output format (`pcd` or `ply`) as the next positional argument; the command will write one pointcloud file per RGB–depth pair (named after the RGB filename).

Segmentation masks are optional. When provided, only foreground pixels (mask value ≠ 1.0) are back-projected into the point cloud.

Camera intrinsics are required via `--intrinsics <calib.json>`. The JSON file supports these layouts:

```jsonc
// Flat keys
{ "fx": 525.0, "fy": 525.0, "cx": 319.5, "cy": 239.5 }

// Nested under "color"
{ "color": { "fx": 525.0, "fy": 525.0, "cx": 319.5, "cy": 239.5 } }

// 3×3 row-major matrix under "K" or "intrinsic_matrix"
{ "K": [525.0, 0, 319.5, 0, 525.0, 239.5, 0, 0, 1] }
```

### 3D Bounding-Box Projection

```bash
# Project metadata bboxes onto a single image or point cloud
./build/main project-bbox <meta.json> <image.png|pointcloud.ply> [output]

# Batch mode: expects color/, meta/, and optionally points/ subdirectories
./build/main project-bbox <base_dir>
```

---

## ROS 2 Bag Reader

Extract topics from a ROS 2 bag to disk (images → PNG, other messages → JSON).

**Requirements:** ROS 2 sourced environment, `rosbag2_py`, `Pillow`.

```bash
# Interactive topic selection
python3 ros2_reader/ros2_bag_reader.py <bag_path> -o <output_dir>

# Specify topics directly
python3 ros2_reader/ros2_bag_reader.py <bag_path> -t /camera/image /imu/data -o <output_dir>
```

---

## Project Structure

```
src/
  include/          # Public headers
    datatype/       # Image, Video, Text, Pointcloud, ImageBuffer
    datasource/     # DataSource abstraction (File, Memory)
    loaders/        # Factory classes for each data type
    utils/          # DataConverter, DataInfo, DataVisualize, FormatDetector
  impl/             # Implementation files
  runner/main.cpp   # CLI entry point
ros2_reader/        # Python ROS 2 bag extraction tool
```