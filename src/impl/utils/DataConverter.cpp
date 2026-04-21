#include "utils/DataConverter.h"
#include "datasource/MemoryDataSource.h"
#include "utils/OpenCVBridge.h"
#include "utils/FFmpegDeleter.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <filesystem>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// ---------------------------------------------------------------------------
// Helpers for writing encoded video into an in-memory buffer
// ---------------------------------------------------------------------------
namespace {

struct WriteContext {
    std::vector<unsigned char> data;
    int64_t pos = 0;
};

static int writePacketCb(void* opaque, uint8_t* buf, int bufSize) {
    auto* ctx = reinterpret_cast<WriteContext*>(opaque);
    int64_t endPos = ctx->pos + bufSize;
    if (endPos > static_cast<int64_t>(ctx->data.size()))
        ctx->data.resize(static_cast<size_t>(endPos));
    std::memcpy(ctx->data.data() + ctx->pos, buf, bufSize);
    ctx->pos += bufSize;
    return bufSize;
}

static int64_t seekOutputCb(void* opaque, int64_t offset, int whence) {
    auto* ctx = reinterpret_cast<WriteContext*>(opaque);
    int64_t size = static_cast<int64_t>(ctx->data.size());
    if (whence == AVSEEK_SIZE) return size;
    int64_t newPos;
    if      (whence == SEEK_SET) newPos = offset;
    else if (whence == SEEK_CUR) newPos = ctx->pos + offset;
    else if (whence == SEEK_END) newPos = size + offset;
    else return -1;
    if (newPos < 0) return -1;
    ctx->pos = newPos;
    return ctx->pos;
}

// RAII wrapper — uses the shared FFmpegDeleter from utils/FFmpegDeleter.h

} // namespace

static std::string toUpper(const std::string& s) {
    std::string upper = s;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return upper;
}

static const char* imageFormatToExtension(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::JPG: return ".jpg";
        case ImageFormat::PNG: return ".png";
        case ImageFormat::EXR: return ".exr";
        default: throw std::runtime_error("Unknown ImageFormat");
    }
}

static const char* videoFormatToFFmpegName(VideoFormat fmt) {
    switch (fmt) {
        case VideoFormat::MP4: return "mp4";
        case VideoFormat::AVI: return "avi";
        case VideoFormat::MKV: return "matroska";
        default: throw std::runtime_error("Unknown VideoFormat");
    }
}

std::shared_ptr<Image> DataConverter::convertImageFormat(
    const std::shared_ptr<Image>& img,
    const std::string& targetFormat
) {
    // Ensure the source image is decoded
    if (img->getImage().empty()) {
        img->load();
    }

    const ImageFormat fmt = imageFormatFromString(toUpper(targetFormat));
    const char* ext = imageFormatToExtension(fmt);

    // Convert ImageBuffer → cv::Mat, encode into the requested format
    cv::Mat mat = OpenCVBridge::bufferToMat(img->getImage());

    // Ensure mat depth is compatible with the target format before encoding
    switch (fmt) {
        case ImageFormat::JPG:
            if (mat.depth() != CV_8U)
                mat.convertTo(mat, CV_8U,
                    mat.depth() == CV_16U ? 1.0 / 256.0 : 255.0);
            break;
        case ImageFormat::PNG:
            if (mat.depth() == CV_32F)
                mat.convertTo(mat, CV_16U, 65535.0);
            break;
        case ImageFormat::EXR:
            if (mat.depth() != CV_32F)
                mat.convertTo(mat, CV_32F,
                    mat.depth() == CV_8U ? 1.0 / 255.0 : 1.0 / 65535.0);
            break;
        default: break;
    }
    std::vector<unsigned char> buf;
    if (!cv::imencode(ext, mat, buf)) {
        throw std::runtime_error("cv::imencode failed for format: " + targetFormat);
    }

    // Keep the encoded bytes in a MemoryDataSource so the Image can be re-loaded
    // or saved later.  Directly set the decoded buffer from the cv::Mat we already
    // hold — avoids an unnecessary decode roundtrip.
    auto memSrc    = std::make_shared<MemoryDataSource>(std::move(buf));
    auto converted = std::make_shared<Image>(memSrc, fmt);
    converted->setImage(OpenCVBridge::matToBuffer(mat));

    return converted;
}

std::shared_ptr<Video> DataConverter::convertVideoFormat(
    const std::shared_ptr<Video>& video,
    const std::string& targetFormat,
    double targetFps
) {
    // Ensure frames are decoded
    if (video->getFrames().empty()) {
        video->load();
    }

    const VideoFormat fmt = videoFormatFromString(toUpper(targetFormat));
    const char* fmtName = videoFormatToFFmpegName(fmt);

    const auto& frames = video->getFrames();
    if (frames.empty())
        throw std::runtime_error("Video has no frames to convert");

    const int    width  = frames[0].width;
    const int    height = frames[0].height;
    const double fps    = targetFps > 0.0 ? targetFps : (video->getFps() > 0.0 ? video->getFps() : 30.0);

    // --- Writable AVIO context backed by an in-memory buffer ---
    WriteContext writeCtx;
    const int avioBufferSize = 65536;
    uint8_t* avioBuffer = reinterpret_cast<uint8_t*>(av_malloc(avioBufferSize));
    if (!avioBuffer)
        throw std::runtime_error("Failed to allocate AVIO buffer");

    AVIOContext* avioCtxRaw = avio_alloc_context(
        avioBuffer, avioBufferSize,
        1,              // write_flag = 1
        &writeCtx,
        nullptr,        // no read callback
        writePacketCb,
        seekOutputCb
    );
    if (!avioCtxRaw) {
        av_free(avioBuffer);
        throw std::runtime_error("Failed to allocate AVIOContext");
    }

    // --- Output format context ---
    AVFormatContext* fmtCtxRaw = nullptr;
    if (avformat_alloc_output_context2(&fmtCtxRaw, nullptr, fmtName, nullptr) < 0 || !fmtCtxRaw)
        throw std::runtime_error("avformat_alloc_output_context2 failed for: " + targetFormat);
    fmtCtxRaw->pb    = avioCtxRaw;
    fmtCtxRaw->flags |= AVFMT_FLAG_CUSTOM_IO;

    // --- Encoder: prefer H.264, fall back to MPEG-4 ---
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!encoder) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("No suitable video encoder found (tried H264, MPEG4)");
    }

    // --- Video stream ---
    AVStream* stream = avformat_new_stream(fmtCtxRaw, nullptr);
    if (!stream) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avformat_new_stream failed");
    }
    stream->id = 0;

    // --- Codec context ---
    std::unique_ptr<AVCodecContext, FFmpegDeleter<AVCodecContext, avcodec_free_context>>
        codecCtx(avcodec_alloc_context3(encoder));
    if (!codecCtx) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avcodec_alloc_context3 failed");
    }
    codecCtx->width     = width;
    codecCtx->height    = height;
    codecCtx->pix_fmt   = AV_PIX_FMT_YUV420P;
    codecCtx->time_base = { 1, static_cast<int>(fps) };
    codecCtx->framerate = { static_cast<int>(fps), 1 };
    codecCtx->gop_size  = 12;
    if (fmtCtxRaw->oformat->flags & AVFMT_GLOBALHEADER)
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(codecCtx.get(), encoder, nullptr) < 0) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avcodec_open2 failed");
    }
    avcodec_parameters_from_context(stream->codecpar, codecCtx.get());
    stream->time_base = codecCtx->time_base;

    // --- Write container header ---
    if (avformat_write_header(fmtCtxRaw, nullptr) < 0) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avformat_write_header failed");
    }

    // --- Allocate encode frames ---
    std::unique_ptr<AVFrame, FFmpegDeleter<AVFrame, av_frame_free>>
        yuvFrame(av_frame_alloc());
    if (!yuvFrame) {
        av_write_trailer(fmtCtxRaw);
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("av_frame_alloc failed");
    }
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width  = width;
    yuvFrame->height = height;
    if (av_frame_get_buffer(yuvFrame.get(), 0) < 0) {
        av_write_trailer(fmtCtxRaw);
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("av_frame_get_buffer failed");
    }

    SwsContext* swsCtx = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!swsCtx) {
        av_write_trailer(fmtCtxRaw);
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("sws_getContext failed");
    }

    std::unique_ptr<AVPacket, FFmpegDeleter<AVPacket, av_packet_free>>
        pkt(av_packet_alloc());
    if (!pkt) {
        sws_freeContext(swsCtx);
        av_write_trailer(fmtCtxRaw);
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("av_packet_alloc failed");
    }

    // Helper: drain encoder
    auto drainEncoder = [&](AVFrame* f) {
        if (avcodec_send_frame(codecCtx.get(), f) < 0)
            throw std::runtime_error("avcodec_send_frame failed");
        while (avcodec_receive_packet(codecCtx.get(), pkt.get()) == 0) {
            av_packet_rescale_ts(pkt.get(), codecCtx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(fmtCtxRaw, pkt.get());
            av_packet_unref(pkt.get());
        }
    };

    // --- Encode each frame ---
    int64_t pts = 0;
    for (const auto& imgBuf : frames) {
        cv::Mat mat = OpenCVBridge::bufferToMat(imgBuf);

        // Ensure 3-channel BGR for swscale
        if (mat.channels() == 1)
            cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
        if (!mat.isContinuous())
            mat = mat.clone();

        // Point a temporary AVFrame at the mat data (no copy)
        const uint8_t* srcData[4] = { mat.data, nullptr, nullptr, nullptr };
        int            srcStride[4] = { static_cast<int>(mat.step), 0, 0, 0 };

        av_frame_make_writable(yuvFrame.get());
        sws_scale(swsCtx, srcData, srcStride, 0, height,
                  yuvFrame->data, yuvFrame->linesize);
        yuvFrame->pts = pts++;
        drainEncoder(yuvFrame.get());
    }

    // Flush encoder
    drainEncoder(nullptr);

    // --- Finalize ---
    sws_freeContext(swsCtx);
    av_write_trailer(fmtCtxRaw);

    // Detach and free AVIO before freeing the format context
    fmtCtxRaw->pb = nullptr;
    av_freep(&avioCtxRaw->buffer);
    avio_context_free(&avioCtxRaw);

    avformat_free_context(fmtCtxRaw);

    // Wrap encoded bytes in a MemoryDataSource and build a new Video
    auto memSrc   = std::make_shared<MemoryDataSource>(std::move(writeCtx.data));
    auto converted = std::make_shared<Video>(memSrc, fmt);
    converted->load();
    return converted;
}

std::shared_ptr<Video> DataConverter::imagesToVideo(
    const std::vector<std::shared_ptr<Image>>& images,
    double fps,
    const std::string& targetFormat
) {
    if (images.empty())
        throw std::runtime_error("imagesToVideo: no images provided");

    // Ensure all images are decoded
    for (const auto& img : images)
        if (img->getImage().empty())
            img->load();

    const VideoFormat fmt      = videoFormatFromString(toUpper(targetFormat));
    const double      actualFps = fps > 0.0 ? fps : 30.0;
    const char*       fmtName  = videoFormatToFFmpegName(fmt);

    cv::Mat first = OpenCVBridge::bufferToMat(images[0]->getImage());
    const int width  = first.cols;
    const int height = first.rows;

    // --- In-memory write context ---
    WriteContext writeCtx;
    const int avioBufferSize = 65536;
    uint8_t* avioBuffer = reinterpret_cast<uint8_t*>(av_malloc(avioBufferSize));
    if (!avioBuffer)
        throw std::runtime_error("Failed to allocate AVIO buffer");

    AVIOContext* avioCtxRaw = avio_alloc_context(
        avioBuffer, avioBufferSize, 1, &writeCtx,
        nullptr, writePacketCb, seekOutputCb);
    if (!avioCtxRaw) {
        av_free(avioBuffer);
        throw std::runtime_error("Failed to allocate AVIOContext");
    }

    // --- Output format context ---
    AVFormatContext* fmtCtxRaw = nullptr;
    if (avformat_alloc_output_context2(&fmtCtxRaw, nullptr, fmtName, nullptr) < 0 || !fmtCtxRaw)
        throw std::runtime_error("avformat_alloc_output_context2 failed");
    fmtCtxRaw->pb    = avioCtxRaw;
    fmtCtxRaw->flags |= AVFMT_FLAG_CUSTOM_IO;

    // --- Encoder ---
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!encoder) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("No suitable video encoder found");
    }

    AVStream* stream = avformat_new_stream(fmtCtxRaw, nullptr);
    if (!stream) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avformat_new_stream failed");
    }
    stream->id = 0;

    std::unique_ptr<AVCodecContext, FFmpegDeleter<AVCodecContext, avcodec_free_context>>
        codecCtx(avcodec_alloc_context3(encoder));
    if (!codecCtx) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avcodec_alloc_context3 failed");
    }
    codecCtx->width     = width;
    codecCtx->height    = height;
    codecCtx->pix_fmt   = AV_PIX_FMT_YUV420P;
    codecCtx->time_base = { 1, static_cast<int>(actualFps) };
    codecCtx->framerate = { static_cast<int>(actualFps), 1 };
    codecCtx->gop_size  = 12;
    if (fmtCtxRaw->oformat->flags & AVFMT_GLOBALHEADER)
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(codecCtx.get(), encoder, nullptr) < 0) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avcodec_open2 failed");
    }
    avcodec_parameters_from_context(stream->codecpar, codecCtx.get());
    stream->time_base = codecCtx->time_base;

    if (avformat_write_header(fmtCtxRaw, nullptr) < 0) {
        avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("avformat_write_header failed");
    }

    std::unique_ptr<AVFrame, FFmpegDeleter<AVFrame, av_frame_free>>
        yuvFrame(av_frame_alloc());
    if (!yuvFrame) {
        av_write_trailer(fmtCtxRaw); avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("av_frame_alloc failed");
    }
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width  = width;
    yuvFrame->height = height;
    if (av_frame_get_buffer(yuvFrame.get(), 0) < 0) {
        av_write_trailer(fmtCtxRaw); avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("av_frame_get_buffer failed");
    }

    SwsContext* swsCtx = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx) {
        av_write_trailer(fmtCtxRaw); avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("sws_getContext failed");
    }

    std::unique_ptr<AVPacket, FFmpegDeleter<AVPacket, av_packet_free>>
        pkt(av_packet_alloc());
    if (!pkt) {
        sws_freeContext(swsCtx);
        av_write_trailer(fmtCtxRaw); avformat_free_context(fmtCtxRaw);
        throw std::runtime_error("av_packet_alloc failed");
    }

    auto drainEncoder = [&](AVFrame* f) {
        if (avcodec_send_frame(codecCtx.get(), f) < 0)
            throw std::runtime_error("avcodec_send_frame failed");
        while (avcodec_receive_packet(codecCtx.get(), pkt.get()) == 0) {
            av_packet_rescale_ts(pkt.get(), codecCtx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(fmtCtxRaw, pkt.get());
            av_packet_unref(pkt.get());
        }
    };

    int64_t pts = 0;
    for (const auto& img : images) {
        cv::Mat mat = OpenCVBridge::bufferToMat(img->getImage());
        if (mat.channels() == 1)
            cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
        else if (mat.channels() == 4)
            cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR);
        if (mat.depth() != CV_8U)
            mat.convertTo(mat, CV_8U,
                mat.depth() == CV_16U ? 1.0 / 256.0 : 255.0);
        if (!mat.isContinuous())
            mat = mat.clone();

        const uint8_t* srcData[4]   = { mat.data, nullptr, nullptr, nullptr };
        int            srcStride[4] = { static_cast<int>(mat.step), 0, 0, 0 };

        av_frame_make_writable(yuvFrame.get());
        sws_scale(swsCtx, srcData, srcStride, 0, height,
                  yuvFrame->data, yuvFrame->linesize);
        yuvFrame->pts = pts++;
        drainEncoder(yuvFrame.get());
    }
    drainEncoder(nullptr);

    sws_freeContext(swsCtx);
    av_write_trailer(fmtCtxRaw);
    fmtCtxRaw->pb = nullptr;
    av_freep(&avioCtxRaw->buffer);
    avio_context_free(&avioCtxRaw);
    avformat_free_context(fmtCtxRaw);

    auto memSrc   = std::make_shared<MemoryDataSource>(std::move(writeCtx.data));
    auto result   = std::make_shared<Video>(memSrc, fmt);
    result->setFps(actualFps);
    result->load();
    return result;
}

// ---------------------------------------------------------------------------
// DataConverter::encodeImagesToFile
// ---------------------------------------------------------------------------
// Encodes a sequence of images directly to a video file on disk using
// cv::VideoWriter. This avoids FFmpeg codec/container compatibility issues
// (H264-in-AVI is poorly supported by most players; VideoWriter uses MJPEG
// for AVI, which is universally readable).
//
// Memory behaviour:
//   • Each image is loaded on demand inside the encoding loop.
//   • Its pixel data is unloaded immediately after the frame is written —
//     peak RAM is one decoded image at a time regardless of how many images
//     are in the input vector.
//   • Images passed in without pre-loaded data benefit the most; pre-loaded
//     images are unloaded by this function after their frame is encoded.
// ---------------------------------------------------------------------------
void DataConverter::encodeImagesToFile(
    const std::vector<std::shared_ptr<Image>>& images,
    double fps,
    const std::string& outputPath
) {
    if (images.empty())
        throw std::runtime_error("encodeImagesToFile: no images provided");

    const double actualFps = fps > 0.0 ? fps : 30.0;

    // Choose fourcc based on extension (same mapping as Video::saveToFile)
    std::string ext = std::filesystem::path(outputPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    int fourcc;
    if      (ext == ".mp4") fourcc = cv::VideoWriter::fourcc('m','p','4','v');
    else if (ext == ".avi") fourcc = cv::VideoWriter::fourcc('M','J','P','G');
    else if (ext == ".mkv") fourcc = cv::VideoWriter::fourcc('H','2','6','4');
    else throw std::runtime_error(
        "encodeImagesToFile: unsupported extension '" + ext +
        "'. Supported: .mp4, .avi, .mkv");

    // Load the first image to read dimensions, then keep it for encoding
    if (images[0]->getImage().empty())
        images[0]->load();
    cv::Mat first = OpenCVBridge::bufferToMat(images[0]->getImage());
    const int width  = first.cols;
    const int height = first.rows;

    cv::VideoWriter writer(outputPath, fourcc, actualFps,
                           cv::Size(width, height));
    if (!writer.isOpened())
        throw std::runtime_error(
            "encodeImagesToFile: failed to open VideoWriter for: " + outputPath);

    for (const auto& img : images) {
        // Load on demand if not already in memory
        if (img->getImage().empty())
            img->load();

        cv::Mat mat = OpenCVBridge::bufferToMat(img->getImage());

        // Normalise to 8-bit BGR for VideoWriter
        if (mat.channels() == 1)       cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
        else if (mat.channels() == 4)  cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR);
        if (mat.depth() != CV_8U)
            mat.convertTo(mat, CV_8U,
                mat.depth() == CV_16U ? 1.0 / 256.0 : 255.0);

        writer.write(mat);

        // Release pixel data immediately — keeps peak RAM to one frame
        img->unload();
    }

    writer.release();
}

std::vector<std::shared_ptr<Image>> DataConverter::videoToImages(
    const std::shared_ptr<Video>& video
) {
    std::vector<std::shared_ptr<Image>> images;

    // Helper: encode one ImageBuffer → PNG Image
    auto makeImage = [](const ImageBuffer& buf) -> std::shared_ptr<Image> {
        cv::Mat mat = OpenCVBridge::bufferToMat(buf);
        std::vector<unsigned char> encoded;
        if (!cv::imencode(".png", mat, encoded))
            throw std::runtime_error("videoToImages: cv::imencode failed for a frame");
        auto memSrc = std::make_shared<MemoryDataSource>(std::move(encoded));
        auto img    = std::make_shared<Image>(memSrc, ImageFormat::PNG);
        img->load();
        return img;
    };

    if (!video->getFrames().empty()) {
        // Fast path: frames already decoded — iterate without re-opening source
        images.reserve(video->getFrames().size());
        for (const auto& buf : video->getFrames())
            images.push_back(makeImage(buf));
    } else {
        // Streaming path: decode one frame at a time via forEachFrame —
        // only one frame's pixel data is in RAM at a time.
        video->forEachFrame([&](std::size_t, const ImageBuffer& buf) {
            images.push_back(makeImage(buf));
        });
    }

    return images;
}

std::shared_ptr<Text> DataConverter::convertTextFormat(
    const std::shared_ptr<Text>& text,
    const std::string& targetFormat
) {
    if (text->getContent().empty())
        text->load();

    const TextFormat fmt = textFormatFromString(toUpper(targetFormat));

    const std::string& content = text->getContent();
    std::vector<unsigned char> bytes(content.begin(), content.end());
    auto memSrc   = std::make_shared<MemoryDataSource>(std::move(bytes));
    auto converted = std::make_shared<Text>(memSrc, fmt);
    converted->load();
    return converted;
}

std::shared_ptr<Pointcloud> DataConverter::convertPointcloudFormat(
    const std::shared_ptr<Pointcloud>& pc,
    const std::string& targetFormat
) {
    if (!pc->isLoaded())
        throw std::runtime_error("convertPointcloudFormat: call load() on the source first");

    const PointcloudFormat fmt = pointcloudFormatFromString(toUpper(targetFormat));

    // Copy geometry and colour in-memory — no re-parsing needed.
    const std::vector<float>& pts = pc->getPoints();
    // MemoryDataSource requires a byte buffer; use a minimal placeholder
    // since we will set the parsed data directly on the new object.
    auto memSrc   = std::make_shared<MemoryDataSource>(std::vector<unsigned char>{});
    auto converted = std::make_shared<Pointcloud>(memSrc, fmt);
    converted->setPoints(pts);
    if (pc->hasColors())
        converted->setColors(pc->getColors(), pc->hasAlpha());
    return converted;
}

std::shared_ptr<Pointcloud> DataConverter::imagesToPointcloud(
    const std::vector<std::shared_ptr<Image>>& rgbs,
    const std::vector<std::shared_ptr<Image>>& depths,
    const std::string& targetFormat,
    const std::string& intrinsicsPath,
    const std::vector<std::shared_ptr<Image>>& segs
) {
    if (rgbs.empty())
        throw std::runtime_error("imagesToPointcloud: no RGB images provided");
    if (intrinsicsPath.empty())
        throw std::runtime_error("imagesToPointcloud: intrinsics JSON path is required");

    // ------------------------------------------------------------------
    // Multi-view / stereo path (multiple RGBs, no depth maps)
    // ------------------------------------------------------------------
    if (depths.empty()) {
        // TODO: implement multi-view / stereo reconstruction
        throw std::runtime_error(
            "imagesToPointcloud: multi-view reconstruction from multiple "
            "RGB images is not yet implemented. Provide aligned depth maps.");
    }

    // ------------------------------------------------------------------
    // RGB-D path: back-project each pixel using a pinhole camera model
    // ------------------------------------------------------------------
    if (rgbs.size() != depths.size())
        throw std::runtime_error(
            "imagesToPointcloud: rgb / depth count mismatch ("
            + std::to_string(rgbs.size()) + " vs "
            + std::to_string(depths.size()) + ")");

    const bool useMask = !segs.empty();
    if (useMask && segs.size() != rgbs.size())
        throw std::runtime_error(
            "imagesToPointcloud: seg mask count mismatch ("
            + std::to_string(segs.size()) + " vs "
            + std::to_string(rgbs.size()) + ")");

    const PointcloudFormat fmt = pointcloudFormatFromString(toUpper(targetFormat));

    std::vector<float>   allPoints;
    std::vector<uint8_t> allColors;

    // Intrinsics: either a single JSON file (reused for every frame) or a
    // directory of per-frame JSON files (one per RGB-D pair, sorted).
    namespace fs = std::filesystem;
    std::vector<std::string> intrinsicFiles;
    const bool perFrameIntrinsics = fs::is_directory(intrinsicsPath);
    if (perFrameIntrinsics) {
        for (const auto& entry : fs::directory_iterator(intrinsicsPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                intrinsicFiles.push_back(entry.path().string());
        }
        std::sort(intrinsicFiles.begin(), intrinsicFiles.end());
        if (intrinsicFiles.empty())
            throw std::runtime_error(
                "imagesToPointcloud: no .json files found in intrinsics directory: "
                + intrinsicsPath);
        if (intrinsicFiles.size() != rgbs.size())
            throw std::runtime_error(
                "imagesToPointcloud: intrinsics file count (" +
                std::to_string(intrinsicFiles.size()) +
                ") does not match frame count (" +
                std::to_string(rgbs.size()) + ")");
    }

    // Helper lambda: parse fx/fy/cx/cy from a JSON object (supports all three layouts)
    auto parseIntrinsicsJson = [](nlohmann::json j, float& fx, float& fy, float& cx, float& cy) {
        // Unwrap optional "color" wrapper
        if (j.contains("color") && j["color"].is_object())
            j = j["color"];

        if (j.contains("fx") && j.contains("fy") &&
            j.contains("cx") && j.contains("cy")) {
            fx = j["fx"].get<float>();
            fy = j["fy"].get<float>();
            cx = j["cx"].get<float>();
            cy = j["cy"].get<float>();
        } else {
            const char* matKey = j.contains("K") ? "K"
                               : j.contains("intrinsic_matrix") ? "intrinsic_matrix"
                               : nullptr;
            if (!matKey)
                throw std::runtime_error(
                    "imagesToPointcloud: intrinsics JSON must contain "
                    "{fx,fy,cx,cy} or a \"K\"/\"intrinsic_matrix\" array");
            auto arr = j[matKey].get<std::vector<float>>();
            if (arr.size() < 9)
                throw std::runtime_error(
                    "imagesToPointcloud: intrinsic matrix must have >= 9 elements");
            fx = arr[0]; fy = arr[4];
            cx = arr[2]; cy = arr[5];
        }
    };

    // Shared intrinsics (single-file mode — populated on first iteration)
    float camFx = 0, camFy = 0, camCx = 0, camCy = 0;

    for (std::size_t i = 0; i < rgbs.size(); ++i) {
        // Ensure images are loaded
        if (rgbs[i]->getImage().empty())   rgbs[i]->load();
        if (depths[i]->getImage().empty()) depths[i]->load();

        cv::Mat rgb   = OpenCVBridge::bufferToMat(rgbs[i]->getImage());
        cv::Mat depth = OpenCVBridge::bufferToMat(depths[i]->getImage());

        if (rgb.rows != depth.rows || rgb.cols != depth.cols)
            throw std::runtime_error(
                "imagesToPointcloud: RGB/depth dimension mismatch at index "
                + std::to_string(i));

        // Convert RGB to 8-bit BGR (OpenCV default) if needed
        if (rgb.depth() != CV_8U)
            rgb.convertTo(rgb, CV_8U,
                rgb.depth() == CV_16U ? 1.0 / 256.0 : 255.0);

        // Convert depth to 32-bit float (metres)
        cv::Mat depthF;
        if (depth.depth() == CV_32F) {
            depthF = depth;
        } else if (depth.depth() == CV_16U) {
            // Common convention: 16-bit depth in millimetres
            depth.convertTo(depthF, CV_32F, 1.0 / 1000.0);
        } else if (depth.depth() == CV_8U) {
            depth.convertTo(depthF, CV_32F);
        } else {
            depth.convertTo(depthF, CV_32F);
        }
        // Use first channel only if multi-channel
        if (depthF.channels() > 1) {
            std::vector<cv::Mat> ch;
            cv::split(depthF, ch);
            depthF = ch[0];
        }

        // Optional segmentation mask – float-based, 1.0 = background
        cv::Mat maskF;
        if (useMask) {
            if (segs[i]->getImage().empty()) segs[i]->load();
            cv::Mat rawMask = OpenCVBridge::bufferToMat(segs[i]->getImage());
            maskF = OpenCVBridge::toFloat1ch(rawMask);
        }

        const int w = rgb.cols;
        const int h = rgb.rows;

        // Camera intrinsics – loaded from the required JSON file.
        // In single-file mode: parse once on i==0, reuse all frames.
        // In per-frame mode: parse the matching file for each frame.
        if (i == 0 || perFrameIntrinsics) {
            const std::string& path = perFrameIntrinsics ? intrinsicFiles[i] : intrinsicsPath;
            std::ifstream ifs(path);
            if (!ifs.is_open())
                throw std::runtime_error(
                    "imagesToPointcloud: cannot open intrinsics file: " + path);
            nlohmann::json j;
            ifs >> j;
            parseIntrinsicsJson(std::move(j), camFx, camFy, camCx, camCy);
        }
        const float fx = camFx;
        const float fy = camFy;
        const float cx = camCx;
        const float cy = camCy;

        for (int v = 0; v < h; ++v) {
            const float* dRow = depthF.ptr<float>(v);
            const float* mRow = useMask ? maskF.ptr<float>(v) : nullptr;
            for (int u = 0; u < w; ++u) {
                // Keep only foreground (masked) pixels; 1.0 = background
                if (mRow && mRow[u] == 1.0f) continue;

                const float z = dRow[u];
                if (z <= 0.0f || !std::isfinite(z)) continue;

                const float x = (static_cast<float>(u) - cx) * z / fx;
                const float y = (static_cast<float>(v) - cy) * z / fy;

                allPoints.push_back(x);
                allPoints.push_back(y);
                allPoints.push_back(z);

                // BGR → RGB
                const cv::Vec3b& bgr = rgb.at<cv::Vec3b>(v, u);
                allColors.push_back(bgr[2]); // R
                allColors.push_back(bgr[1]); // G
                allColors.push_back(bgr[0]); // B
            }
        }
    }

    auto memSrc    = std::make_shared<MemoryDataSource>(std::vector<unsigned char>{});
    auto converted = std::make_shared<Pointcloud>(memSrc, fmt);
    converted->setPoints(std::move(allPoints));
    if (!allColors.empty())
        converted->setColors(std::move(allColors), /*alpha=*/false);
    return converted;
}