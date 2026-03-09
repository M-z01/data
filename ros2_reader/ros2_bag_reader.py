import os
import json
import argparse
import numpy as np
from pathlib import Path
from datetime import datetime

try:
    import rclpy
    from rclpy.serialization import deserialize_message
    from rosidl_runtime_py.utilities import get_message
    import rosbag2_py
except ImportError:
    print("Ensure ROS2 is sourced and rosbag2_py is available.")
    exit(1)

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install Pillow")
    exit(1)


def get_available_topics(bag_path: str) -> dict:
    """List all topics and their types in the bag."""
    reader = rosbag2_py.SequentialReader()
    storage_options = rosbag2_py.StorageOptions(uri=bag_path, storage_id="sqlite3")
    converter_options = rosbag2_py.ConverterOptions(
        input_serialization_format="cdr",
        output_serialization_format="cdr"
    )
    reader.open(storage_options, converter_options)
    topic_types = reader.get_all_topics_and_types()
    return {t.name: t.type for t in topic_types}


def sanitize_topic_name(topic: str) -> str:
    """Convert topic name to a safe folder name."""
    return topic.lstrip("/").replace("/", "_")


def is_image_topic(msgtype: str) -> bool:
    return "Image" in msgtype or "CompressedImage" in msgtype


def decode_image(msg, msgtype: str) -> Image.Image:
    """Decode a ROS image message to a PIL Image."""
    if "CompressedImage" in msgtype:
        import io
        return Image.open(io.BytesIO(bytes(msg.data)))

    encoding = msg.encoding
    width = msg.width
    height = msg.height

    if encoding in ("rgb8", "bgr8", "rgba8", "bgra8"):
        data = np.frombuffer(bytes(msg.data), dtype=np.uint8)
        channels = 4 if "a" in encoding else 3
        data = data.reshape((height, width, channels))
        if encoding.startswith("bgr"):
            data = data[..., ::-1] if channels == 3 else np.concatenate(
                [data[..., 2::-1], data[..., 3:4]], axis=-1)
        return Image.fromarray(data, "RGB" if channels == 3 else "RGBA")
    elif encoding == "mono8":
        data = np.frombuffer(bytes(msg.data), dtype=np.uint8).reshape((height, width))
        return Image.fromarray(data, "L")
    elif encoding in ("mono16", "16UC1"):
        # 16-bit single channel — preserve full 16-bit depth as PNG
        data = np.frombuffer(bytes(msg.data), dtype=np.uint16).reshape((height, width))
        return Image.fromarray(data, "I;16")
    elif encoding == "32FC1":
        # 32-bit float single channel — normalize to 16-bit for PNG
        data = np.frombuffer(bytes(msg.data), dtype=np.float32).reshape((height, width))
        valid = data[np.isfinite(data)]
        if valid.size > 0:
            min_val, max_val = valid.min(), valid.max()
            if max_val > min_val:
                data = np.clip((data - min_val) / (max_val - min_val) * 65535, 0, 65535)
            else:
                data = np.zeros_like(data)
        else:
            data = np.zeros_like(data)
        return Image.fromarray(data.astype(np.uint16), "I;16")
    else:
        raise ValueError(f"Unsupported encoding: {encoding}")


def msg_to_dict(msg) -> dict:
    """Recursively convert a ROS message to a dict."""
    if hasattr(msg, "get_fields_and_field_types"):
        result = {}
        for field in msg.get_fields_and_field_types():
            value = getattr(msg, field)
            result[field] = msg_to_dict(value)
        return result
    elif isinstance(msg, (list, tuple, np.ndarray)):
        return [msg_to_dict(v) for v in msg]
    elif isinstance(msg, (int, float, str, bool, type(None))):
        return msg
    elif isinstance(msg, bytes):
        return list(msg)
    else:
        try:
            return str(msg)
        except Exception:
            return None


def read_bag(bag_path: str, selected_topics: list, output_dir: str):
    """Read selected topics from bag and save to output directory."""
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    # Resolve topic -> msgtype map
    all_topics = get_available_topics(bag_path)

    topic_dirs = {}
    topic_indices = {}
    topic_msgtypes = {}
    topic_type_map = {}

    for topic in selected_topics:
        if topic not in all_topics:
            print(f"[WARN] Topic '{topic}' not found in bag. Skipping.")
            continue
        msgtype_str = all_topics[topic]
        try:
            msg_class = get_message(msgtype_str)
        except Exception as e:
            print(f"[WARN] Could not resolve message type '{msgtype_str}': {e}. Skipping.")
            continue

        safe_name = sanitize_topic_name(topic)
        topic_dir = os.path.join(output_dir, safe_name)
        os.makedirs(topic_dir, exist_ok=True)

        topic_dirs[topic] = topic_dir
        topic_indices[topic] = []
        topic_msgtypes[topic] = msgtype_str
        topic_type_map[topic] = msg_class
        print(f"[INFO] Output for '{topic}' -> {topic_dir}")

    if not topic_dirs:
        print("[ERROR] No valid topics selected.")
        return

    # Open reader with topic filter
    reader = rosbag2_py.SequentialReader()
    storage_options = rosbag2_py.StorageOptions(uri=bag_path, storage_id="sqlite3")
    converter_options = rosbag2_py.ConverterOptions(
        input_serialization_format="cdr",
        output_serialization_format="cdr"
    )
    reader.open(storage_options, converter_options)

    filter_ = rosbag2_py.StorageFilter(topics=list(topic_dirs.keys()))
    reader.set_filter(filter_)

    while reader.has_next():
        topic, rawdata, timestamp_ns = reader.read_next()

        if topic not in topic_dirs:
            continue

        msgtype = topic_msgtypes[topic]
        msg_class = topic_type_map[topic]
        msg = deserialize_message(rawdata, msg_class)

        timestamp_sec = timestamp_ns / 1e9
        dt_str = datetime.utcfromtimestamp(timestamp_sec).strftime("%Y%m%d_%H%M%S_%f")
        topic_dir = topic_dirs[topic]

        if is_image_topic(msgtype):
            try:
                img = decode_image(msg, msgtype)
                filename = f"{dt_str}.png"
                filepath = os.path.join(topic_dir, filename)
                img.save(filepath)
                topic_indices[topic].append({
                    "timestamp_ns": timestamp_ns,
                    "timestamp_sec": timestamp_sec,
                    "file": filename
                })
            except Exception as e:
                print(f"[WARN] Failed to decode image at {timestamp_ns}: {e}")
        else:
            try:
                data = msg_to_dict(msg)
                filename = f"{dt_str}.json"
                filepath = os.path.join(topic_dir, filename)
                record = {
                    "timestamp_ns": timestamp_ns,
                    "timestamp_sec": timestamp_sec,
                    "data": data
                }
                with open(filepath, "w") as f:
                    json.dump(record, f, indent=2)
                topic_indices[topic].append({
                    "timestamp_ns": timestamp_ns,
                    "timestamp_sec": timestamp_sec,
                    "file": filename
                })
            except Exception as e:
                print(f"[WARN] Failed to serialize message at {timestamp_ns}: {e}")

    # Write index files per topic
    for topic, entries in topic_indices.items():
        index_path = os.path.join(topic_dirs[topic], "index.json")
        with open(index_path, "w") as f:
            json.dump({
                "topic": topic,
                "msgtype": topic_msgtypes[topic],
                "total_messages": len(entries),
                "messages": entries
            }, f, indent=2)
        print(f"[INFO] '{topic}': {len(entries)} messages saved. Index -> {index_path}")


def interactive_topic_selection(topics: dict) -> list:
    """Prompt user to select topics interactively."""
    print("\nAvailable topics:")
    topic_list = list(topics.items())
    for i, (topic, msgtype) in enumerate(topic_list):
        print(f"  [{i}] {topic}  ({msgtype})")

    print("\nEnter topic numbers separated by commas, or 'all' to select all:")
    choice = input("> ").strip()

    if choice.lower() == "all":
        return [t for t, _ in topic_list]

    selected = []
    for part in choice.split(","):
        part = part.strip()
        if part.isdigit():
            idx = int(part)
            if 0 <= idx < len(topic_list):
                selected.append(topic_list[idx][0])
            else:
                print(f"[WARN] Index {idx} out of range, skipping.")
        else:
            print(f"[WARN] Invalid input '{part}', skipping.")
    return selected


def main():
    rclpy.init()

    parser = argparse.ArgumentParser(description="ROS2 Bag Reader")
    parser.add_argument("bag_path", help="Path to the ROS2 bag folder")
    parser.add_argument("-o", "--output", default="./bag_output",
                        help="Output directory (default: ./bag_output)")
    parser.add_argument("-t", "--topics", nargs="*",
                        help="Topics to extract (e.g. /camera/image /imu/data). "
                             "If omitted, interactive selection is used.")
    args = parser.parse_args()

    if not os.path.exists(args.bag_path):
        print(f"[ERROR] Bag path not found: {args.bag_path}")
        rclpy.shutdown()
        exit(1)

    print(f"[INFO] Reading bag: {args.bag_path}")
    topics = get_available_topics(args.bag_path)

    if not topics:
        print("[ERROR] No topics found in bag.")
        rclpy.shutdown()
        exit(1)

    if args.topics:
        selected = args.topics
    else:
        selected = interactive_topic_selection(topics)

    if not selected:
        print("[ERROR] No topics selected.")
        rclpy.shutdown()
        exit(1)

    print(f"\n[INFO] Selected topics: {selected}")
    read_bag(args.bag_path, selected, args.output)
    print("\n[DONE] Extraction complete.")

    rclpy.shutdown()


if __name__ == "__main__":
    main()