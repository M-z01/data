#include "datatype/Video.h"
#include "utils/OpenCVBridge.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// ---------------------------------------------------------------------------
// Custom AVIOContext — reads directly from std::vector<unsigned char>
// ---------------------------------------------------------------------------
struct BufferContext {
    const std::vector<unsigned char>* data;
    int64_t pos;
};

static int readPacket(void* opaque, uint8_t* buf, int bufSize) {
    auto* ctx = reinterpret_cast<BufferContext*>(opaque);
    int64_t remaining = static_cast<int64_t>(ctx->data->size()) - ctx->pos;
    if (remaining <= 0) return AVERROR_EOF;
    int toRead = static_cast<int>(std::min(static_cast<int64_t>(bufSize), remaining));
    std::memcpy(buf, ctx->data->data() + ctx->pos, toRead);
    ctx->pos += toRead;
    return toRead;
}

static int64_t seekPacket(void* opaque, int64_t offset, int whence) {
    auto* ctx = reinterpret_cast<BufferContext*>(opaque);
    int64_t size = static_cast<int64_t>(ctx->data->size());
    if (whence == AVSEEK_SIZE) return size;
    int64_t newPos;
    if      (whence == SEEK_SET) newPos = offset;
    else if (whence == SEEK_CUR) newPos = ctx->pos + offset;
    else if (whence == SEEK_END) newPos = size + offset;
    else return -1;
    if (newPos < 0 || newPos > size) return -1;
    ctx->pos = newPos;
    return ctx->pos;
}

// ---------------------------------------------------------------------------
// RAII helper — wraps any FFmpeg free function of the form void f(T**)
// ---------------------------------------------------------------------------
template<typename T, void (*Fn)(T**)>
struct FFmpegDeleter {
    void operator()(T* p) const { if (p) Fn(&p); }
};

// ---------------------------------------------------------------------------
// Video::load  — zero disk I/O
// ---------------------------------------------------------------------------
void Video::load() {
    if (format == VideoFormat::UNKNOWN)
        throw std::runtime_error("Video format not set before load()");

    std::vector<unsigned char> bytes = source->getRawBytes();
    if (bytes.empty())
        throw std::runtime_error("Empty data from source");

    // --- Custom AVIO context wrapping the in-memory buffer ---
    const int avioBufferSize = 65536;
    uint8_t* avioBuffer = reinterpret_cast<uint8_t*>(av_malloc(avioBufferSize));
    if (!avioBuffer)
        throw std::runtime_error("Failed to allocate AVIO buffer");

    BufferContext bufCtx{ &bytes, 0 };
    AVIOContext* avioCtxRaw = avio_alloc_context(
        avioBuffer, avioBufferSize,
        0,           // write_flag = 0 (read-only)
        &bufCtx,
        readPacket,
        nullptr,     // no write callback
        seekPacket
    );
    if (!avioCtxRaw) {
        av_free(avioBuffer);
        throw std::runtime_error("Failed to allocate AVIOContext");
    }
    std::unique_ptr<AVIOContext, void(*)(AVIOContext*)> avioPtr(
        avioCtxRaw,
        [](AVIOContext* c) { av_freep(&c->buffer); avio_context_free(&c); }
    );

    // --- Format context ---
    AVFormatContext* fmtCtxRaw = avformat_alloc_context();
    if (!fmtCtxRaw)
        throw std::runtime_error("Failed to allocate AVFormatContext");
    fmtCtxRaw->pb = avioCtxRaw;

    if (avformat_open_input(&fmtCtxRaw, nullptr, nullptr, nullptr) < 0)
        throw std::runtime_error("avformat_open_input failed");
    // avformat_open_input may set fmtCtxRaw to null on failure, but throws above.
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> fmtPtr(
        fmtCtxRaw,
        [](AVFormatContext* f) { avformat_close_input(&f); }
    );

    if (avformat_find_stream_info(fmtCtxRaw, nullptr) < 0)
        throw std::runtime_error("avformat_find_stream_info failed");

    // --- Locate first video stream ---
    int videoIdx = -1;
    const AVCodec* codec = nullptr;
    AVStream* videoStream = nullptr;
    for (unsigned i = 0; i < fmtCtxRaw->nb_streams; ++i) {
        if (fmtCtxRaw->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = static_cast<int>(i);
            videoStream = fmtCtxRaw->streams[i];
            codec = avcodec_find_decoder(fmtCtxRaw->streams[i]->codecpar->codec_id);
            break;
        }
    }
    if (videoIdx < 0 || !codec)
        throw std::runtime_error("No video stream found");

    if (videoStream->avg_frame_rate.den > 0) {
        fps = av_q2d(videoStream->avg_frame_rate);
    } else if (videoStream->r_frame_rate.den > 0) {
        fps = av_q2d(videoStream->r_frame_rate);
    } else {
        fps = 30.0;
    }

    // --- Codec context ---
    std::unique_ptr<AVCodecContext, FFmpegDeleter<AVCodecContext, avcodec_free_context>>
        codecCtx(avcodec_alloc_context3(codec));
    if (!codecCtx)
        throw std::runtime_error("avcodec_alloc_context3 failed");

    avcodec_parameters_to_context(codecCtx.get(), fmtCtxRaw->streams[videoIdx]->codecpar);
    if (avcodec_open2(codecCtx.get(), codec, nullptr) < 0)
        throw std::runtime_error("avcodec_open2 failed");

    // --- Decode loop ---
    std::unique_ptr<AVPacket, FFmpegDeleter<AVPacket, av_packet_free>>
        pkt(av_packet_alloc());
    std::unique_ptr<AVFrame, FFmpegDeleter<AVFrame, av_frame_free>>
        frame(av_frame_alloc());
    std::unique_ptr<AVFrame, FFmpegDeleter<AVFrame, av_frame_free>>
        bgrFrame(av_frame_alloc());

    if (!pkt || !frame || !bgrFrame)
        throw std::runtime_error("Failed to allocate AVPacket/AVFrame");

    // swsCtx is kept raw because sws_freeContext does not take T**.
    SwsContext* swsCtx = nullptr;

    auto decodeFrame = [&](AVFrame* f) {
        // Re-initialise on first call or if the video resolution changes mid-stream.
        if (!swsCtx || bgrFrame->width != f->width || bgrFrame->height != f->height) {
            if (swsCtx)          sws_freeContext(swsCtx);
            if (bgrFrame->data[0]) av_freep(&bgrFrame->data[0]);

            swsCtx = sws_getContext(
                f->width, f->height,
                static_cast<AVPixelFormat>(f->format),
                f->width, f->height,
                AV_PIX_FMT_BGR24,
                SWS_BILINEAR, nullptr, nullptr, nullptr
            );
            if (!swsCtx)
                throw std::runtime_error("sws_getContext failed");

            if (av_image_alloc(bgrFrame->data, bgrFrame->linesize,
                               f->width, f->height, AV_PIX_FMT_BGR24, 1) < 0)
                throw std::runtime_error("av_image_alloc failed");

            bgrFrame->width  = f->width;
            bgrFrame->height = f->height;
        }

        sws_scale(swsCtx,
                  f->data, f->linesize, 0, f->height,
                  bgrFrame->data, bgrFrame->linesize);
        // matToBuffer copies pixel data; bgrFrame buffer is safely reused next call.
        cv::Mat mat(f->height, f->width, CV_8UC3,
                    bgrFrame->data[0], bgrFrame->linesize[0]);
        frames.push_back(OpenCVBridge::matToBuffer(mat));
    };

    while (av_read_frame(fmtCtxRaw, pkt.get()) >= 0) {
        if (pkt->stream_index == videoIdx) {
            if (avcodec_send_packet(codecCtx.get(), pkt.get()) == 0)
                while (avcodec_receive_frame(codecCtx.get(), frame.get()) == 0)
                    decodeFrame(frame.get());
        }
        av_packet_unref(pkt.get());
    }

    // Flush decoder
    avcodec_send_packet(codecCtx.get(), nullptr);
    while (avcodec_receive_frame(codecCtx.get(), frame.get()) == 0)
        decodeFrame(frame.get());

    // --- Cleanup of non-smart-pointer resources ---
    if (swsCtx)            sws_freeContext(swsCtx);
    if (bgrFrame->data[0]) av_freep(&bgrFrame->data[0]);
    // All unique_ptrs (codecCtx, fmtPtr, avioPtr, pkt, frame, bgrFrame)
    // are destroyed automatically in reverse declaration order.
}
void Video::saveToFile(const std::string& path, VideoFormat fmt) const {
    if (frames.empty())
        throw std::runtime_error("Cannot save: video has no frames");

    int fourcc;
    switch (fmt) {
        case VideoFormat::MP4: fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v'); break;
        case VideoFormat::AVI: fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G'); break;
        case VideoFormat::MKV: fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4'); break;
        default: throw std::runtime_error("Cannot save: video format is UNKNOWN");
    }

    cv::Mat firstFrame = OpenCVBridge::bufferToMat(frames[0]);
    double fpsToUse = (fps > 0.0) ? fps : 30.0;
    cv::VideoWriter writer(path, fourcc, fpsToUse, cv::Size(firstFrame.cols, firstFrame.rows));

    if (!writer.isOpened())
        throw std::runtime_error("Failed to open VideoWriter for path: " + path);

    for (const auto& buffer : frames) {
        cv::Mat mat = OpenCVBridge::bufferToMat(buffer);
        writer.write(mat);
    }
}

void Video::saveToFile(const std::string& path) const {
    saveToFile(path, format);
}

