#include "utils/DataConverter.h"
#include "datasource/MemoryDataSource.h"
#include "utils/OpenCVBridge.h"
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstring>

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

// RAII wrapper matching the pattern in Video.cpp
template<typename T, void (*Fn)(T**)>
struct FFmpegDeleter {
    void operator()(T* p) const { if (p) Fn(&p); }
};

} // namespace

// Map a user-supplied format string (case-insensitive) to an OpenCV file extension.
static std::string formatToExtension(const std::string& fmt) {
    std::string upper = fmt;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "JPG" || upper == "JPEG") return ".jpg";
    if (upper == "PNG")                    return ".png";
    if (upper == "EXR")                    return ".exr";

    throw std::invalid_argument("Unsupported target format: " + fmt +
                                ". Supported: JPG, JPEG, PNG, EXR");
}

std::shared_ptr<Image> DataConverter::convertImageFormat(
    const std::shared_ptr<Image>& img,
    const std::string& targetFormat
) {
    // Ensure the source image is decoded
    if (img->getImage().empty()) {
        img->load();
    }

    const std::string ext = formatToExtension(targetFormat);

    // Convert ImageBuffer → cv::Mat, encode into the requested format
    cv::Mat mat = OpenCVBridge::bufferToMat(img->getImage());
    std::vector<unsigned char> buf;
    if (!cv::imencode(ext, mat, buf)) {
        throw std::runtime_error("cv::imencode failed for format: " + targetFormat);
    }

    // Wrap the encoded bytes in a MemoryDataSource and build a new Image
    auto memSrc   = std::make_shared<MemoryDataSource>(std::move(buf));
    auto converted = std::make_shared<Image>(memSrc);
    converted->load(); // decode & detect format

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

    // Map format string (case-insensitive) to FFmpeg muxer name
    std::string upper = targetFormat;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    const char* fmtName = nullptr;
    if      (upper == "MP4") fmtName = "mp4";
    else if (upper == "AVI") fmtName = "avi";
    else if (upper == "MKV") fmtName = "matroska";
    else throw std::invalid_argument("Unsupported target video format: " + targetFormat +
                                     ". Supported: MP4, AVI, MKV");

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
    auto converted = std::make_shared<Video>(memSrc);
    converted->load(); // decode & detect format
    return converted;
}

std::shared_ptr<Video> DataConverter::imagesToVideo(
    const std::vector<std::shared_ptr<Image>>& images,
    double fps
) {
    // TODO: implement
    throw std::runtime_error("imagesToVideo not yet implemented");
}

std::vector<std::shared_ptr<Image>> DataConverter::videoToImages(
    const std::shared_ptr<Video>& video
) {
    // TODO: implement
    throw std::runtime_error("videoToImages not yet implemented");
}