#include "datatype/Video.h"
#include "utils/OpenCVBridge.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <memory>
#include <functional>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// ---------------------------------------------------------------------------
// RAII helper — wraps any FFmpeg free function of the form void f(T**)
// ---------------------------------------------------------------------------
template<typename T, void (*Fn)(T**)>
struct FFmpegDeleter {
    void operator()(T* p) const { if (p) Fn(&p); }
};

// ---------------------------------------------------------------------------
// Custom AVIO callbacks (used only for memory-backed sources)
// ---------------------------------------------------------------------------
struct BufferContext {
    const std::vector<unsigned char>* data;
    int64_t pos;
};

static int avioRead(void* opaque, uint8_t* buf, int bufSize) {
    auto* ctx = reinterpret_cast<BufferContext*>(opaque);
    int64_t remaining = static_cast<int64_t>(ctx->data->size()) - ctx->pos;
    if (remaining <= 0) return AVERROR_EOF;
    int toRead = static_cast<int>(std::min(static_cast<int64_t>(bufSize), remaining));
    std::memcpy(buf, ctx->data->data() + ctx->pos, toRead);
    ctx->pos += toRead;
    return toRead;
}

static int64_t avioSeek(void* opaque, int64_t offset, int whence) {
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
// SourceHandle — owns an open AVFormatContext (from file path or memory AVIO)
// Must be closed with closeSource() when done.
// ---------------------------------------------------------------------------
struct SourceHandle {
    AVFormatContext* fmtCtx  = nullptr;
    AVIOContext*     avioCtx = nullptr;
    // Kept alive while avioCtx references them (memory-backed sources only)
    std::vector<unsigned char> bytes;
    BufferContext              bufCtx{nullptr, 0};
};

static SourceHandle openSource(DataSource* source) {
    SourceHandle h;
    const std::string path = source->getPath();

    if (!path.empty()) {
        // ---- File-backed: let FFmpeg open the file directly ----
        // No copy of the encoded data; FFmpeg reads chunks via its own buffering.
        if (avformat_open_input(&h.fmtCtx, path.c_str(), nullptr, nullptr) < 0)
            throw std::runtime_error("avformat_open_input failed: " + path);
    } else {
        // ---- Memory-backed: wire custom AVIO around the byte vector ----
        h.bytes = source->getRawBytes();
        if (h.bytes.empty())
            throw std::runtime_error("Empty data from source");

        const int avioBufSize = 65536;
        uint8_t* avioBuf = reinterpret_cast<uint8_t*>(av_malloc(avioBufSize));
        if (!avioBuf)
            throw std::runtime_error("Failed to allocate AVIO buffer");

        h.bufCtx  = {&h.bytes, 0};
        h.avioCtx = avio_alloc_context(
            avioBuf, avioBufSize, 0, &h.bufCtx,
            avioRead, nullptr, avioSeek);
        if (!h.avioCtx) {
            av_free(avioBuf);
            throw std::runtime_error("Failed to allocate AVIOContext");
        }

        h.fmtCtx = avformat_alloc_context();
        if (!h.fmtCtx) {
            av_freep(&h.avioCtx->buffer);
            avio_context_free(&h.avioCtx);
            throw std::runtime_error("Failed to allocate AVFormatContext");
        }
        h.fmtCtx->pb = h.avioCtx;

        if (avformat_open_input(&h.fmtCtx, nullptr, nullptr, nullptr) < 0) {
            h.fmtCtx = nullptr; // freed by avformat_open_input on failure
            av_freep(&h.avioCtx->buffer);
            avio_context_free(&h.avioCtx);
            throw std::runtime_error("avformat_open_input failed (memory source)");
        }
    }
    return h;
}

static void closeSource(SourceHandle& h) {
    if (h.fmtCtx)  avformat_close_input(&h.fmtCtx);
    if (h.avioCtx) {
        av_freep(&h.avioCtx->buffer);
        avio_context_free(&h.avioCtx);
    }
}

// ---------------------------------------------------------------------------
// runDecodeLoop — core FFmpeg decode loop
//
// Finds the first video stream, opens the decoder, and calls onFrame(mat)
// for each decoded BGR8 frame. The cv::Mat is a view into an internal
// temporary buffer — it is only valid for the duration of the callback.
// onFrame must copy pixel data if it needs to keep it.
// outFps is set to the detected frame rate.
// ---------------------------------------------------------------------------
static void runDecodeLoop(
    AVFormatContext* fmtCtx,
    double& outFps,
    std::function<void(cv::Mat&)> onFrame)
{
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
        throw std::runtime_error("avformat_find_stream_info failed");

    int videoIdx = -1;
    const AVCodec* codec = nullptr;
    AVStream* videoStream = nullptr;
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx    = static_cast<int>(i);
            videoStream = fmtCtx->streams[i];
            codec       = avcodec_find_decoder(fmtCtx->streams[i]->codecpar->codec_id);
            break;
        }
    }
    if (videoIdx < 0 || !codec)
        throw std::runtime_error("No video stream found");

    if (videoStream->avg_frame_rate.den > 0)
        outFps = av_q2d(videoStream->avg_frame_rate);
    else if (videoStream->r_frame_rate.den > 0)
        outFps = av_q2d(videoStream->r_frame_rate);
    else
        outFps = 30.0;

    std::unique_ptr<AVCodecContext, FFmpegDeleter<AVCodecContext, avcodec_free_context>>
        codecCtx(avcodec_alloc_context3(codec));
    if (!codecCtx) throw std::runtime_error("avcodec_alloc_context3 failed");

    avcodec_parameters_to_context(codecCtx.get(), fmtCtx->streams[videoIdx]->codecpar);
    if (avcodec_open2(codecCtx.get(), codec, nullptr) < 0)
        throw std::runtime_error("avcodec_open2 failed");

    std::unique_ptr<AVPacket, FFmpegDeleter<AVPacket, av_packet_free>> pkt(av_packet_alloc());
    std::unique_ptr<AVFrame,  FFmpegDeleter<AVFrame,  av_frame_free>>  frame(av_frame_alloc());
    std::unique_ptr<AVFrame,  FFmpegDeleter<AVFrame,  av_frame_free>>  bgrFrame(av_frame_alloc());
    if (!pkt || !frame || !bgrFrame)
        throw std::runtime_error("Failed to allocate AVPacket/AVFrame");

    SwsContext* swsCtx = nullptr;

    auto processFrame = [&](AVFrame* f) {
        // Re-initialise swscale on first call or resolution change
        if (!swsCtx || bgrFrame->width != f->width || bgrFrame->height != f->height) {
            if (swsCtx)            sws_freeContext(swsCtx);
            if (bgrFrame->data[0]) av_freep(&bgrFrame->data[0]);

            swsCtx = sws_getContext(
                f->width, f->height, static_cast<AVPixelFormat>(f->format),
                f->width, f->height, AV_PIX_FMT_BGR24,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!swsCtx) throw std::runtime_error("sws_getContext failed");

            if (av_image_alloc(bgrFrame->data, bgrFrame->linesize,
                               f->width, f->height, AV_PIX_FMT_BGR24, 1) < 0)
                throw std::runtime_error("av_image_alloc failed");

            bgrFrame->width  = f->width;
            bgrFrame->height = f->height;
        }
        sws_scale(swsCtx,
                  f->data, f->linesize, 0, f->height,
                  bgrFrame->data, bgrFrame->linesize);

        // Wrap bgrFrame in a cv::Mat view (no copy) and hand to caller
        cv::Mat mat(f->height, f->width, CV_8UC3,
                    bgrFrame->data[0], bgrFrame->linesize[0]);
        onFrame(mat);
    };

    while (av_read_frame(fmtCtx, pkt.get()) >= 0) {
        if (pkt->stream_index == videoIdx) {
            if (avcodec_send_packet(codecCtx.get(), pkt.get()) == 0)
                while (avcodec_receive_frame(codecCtx.get(), frame.get()) == 0)
                    processFrame(frame.get());
        }
        av_packet_unref(pkt.get());
    }
    // Flush decoder
    avcodec_send_packet(codecCtx.get(), nullptr);
    while (avcodec_receive_frame(codecCtx.get(), frame.get()) == 0)
        processFrame(frame.get());

    if (swsCtx)            sws_freeContext(swsCtx);
    if (bgrFrame->data[0]) av_freep(&bgrFrame->data[0]);
}

// ---------------------------------------------------------------------------
// Video::load  — eager: decodes all frames into this->frames
// ---------------------------------------------------------------------------
void Video::load() {
    if (format == VideoFormat::UNKNOWN)
        throw std::runtime_error("Video format not set before load()");

    SourceHandle h = openSource(source.get());
    try {
        runDecodeLoop(h.fmtCtx, fps, [this](cv::Mat& mat) {
            frames.push_back(OpenCVBridge::matToBuffer(mat));
        });
    } catch (...) {
        closeSource(h);
        throw;
    }
    closeSource(h);
    // Populate lightweight metadata from the first decoded frame
    if (!frames.empty()) {
        metaWidth    = frames[0].width;
        metaHeight   = frames[0].height;
        metaChannels = frames[0].channels;
    }
    loaded_ = true;
}

// ---------------------------------------------------------------------------
// Video::scanMetadata  — reads fps + dimensions without storing any frames
//
// Opens the source, finds the video stream, decodes exactly one frame to
// populate fps/metaWidth/metaHeight/metaChannels, then closes.
// Use this before forEachFrame() when you need metadata but cannot afford
// to call load() (which would hold all frames in RAM).
// ---------------------------------------------------------------------------
void Video::scanMetadata() {
    SourceHandle h = openSource(source.get());
    bool gotOne = false;
    try {
        runDecodeLoop(h.fmtCtx, fps, [this, &gotOne](cv::Mat& mat) {
            if (!gotOne) {
                metaWidth    = mat.cols;
                metaHeight   = mat.rows;
                metaChannels = mat.channels();
                gotOne       = true;
            }
            // Don't store the frame — throw a sentinel to exit early
            throw std::runtime_error("__scanMetadata_done__");
        });
    } catch (const std::runtime_error& e) {
        closeSource(h);
        if (std::string(e.what()) != "__scanMetadata_done__") throw;
        return;
    } catch (...) {
        closeSource(h);
        throw;
    }
    closeSource(h);
}

// ---------------------------------------------------------------------------
// Video::forEachFrame  — streaming: one frame in RAM at a time
//
// Does NOT populate this->frames. The callback receives a temporary
// ImageBuffer that is valid only during the call — copy it if you need it.
// ---------------------------------------------------------------------------
void Video::forEachFrame(FrameCallback cb) const {
    SourceHandle h = openSource(source.get());
    double detectedFps = fps; // ignored output; fps already set after load()
    std::size_t idx = 0;
    try {
        runDecodeLoop(h.fmtCtx, detectedFps, [&](cv::Mat& mat) {
            ImageBuffer buf = OpenCVBridge::matToBuffer(mat);
            cb(idx++, buf);
        });
    } catch (...) {
        closeSource(h);
        throw;
    }
    closeSource(h);
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

