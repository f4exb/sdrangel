///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>

#include "libswscale/swscale.h"
}


#include <opencv2/opencv.hpp>  // Add OpenCV for text overlay

#include <QDateTime>

#include "tsgenerator.h"

const double  TSGenerator::rate_qpsk[11][4] = {{1.0, 4.0, 12.0, 2}, {1.0, 3.0, 12.0, 2}, {2.0, 5.0, 12.0, 2}, {1.0, 2.0, 12.0, 2}, {3.0, 5.0, 12.0, 2}, {2.0, 3.0, 10.0, 2}, {3.0, 4.0, 12.0, 2}, {4.0, 5.0, 12.0, 2}, {5.0, 6.0, 10.0, 2}, {8.0, 9.0, 8.0, 2}, {9.0, 10.0, 8.0, 1}};
const double  TSGenerator::rate_8psk[6][4] = {{3.0, 5.0, 12.0, 2}, {2.0, 3.0, 10.0, 2}, {3.0, 4.0, 12.0, 2}, {5.0, 6.0, 10.0, 2}, {8.0, 9.0, 8.0, 2}, {9.0, 10.0, 8.0, 1}};
const double  TSGenerator::rate_16apsk[6][4] = {{2.0, 3.0, 10.0, 2}, {3.0, 4.0, 12.0, 2}, {4.0, 5.0, 12.0, 2}, {5.0, 6.0, 10.0, 2}, {8.0, 9.0, 8.0, 2}, {9.0, 10.0, 8.0, 1}};
const double  TSGenerator::rate_32apsk[5][4] = {{3.0, 4.0, 12.0, 2}, {4.0, 5.0, 12.0, 2}, {5.0, 6.0, 10.0, 2}, {8.0, 9.0, 8.0, 2}, {9.0, 10.0, 8.0, 1}};

uint8_t TSGenerator::continuity_counters[8192] = {0};

TSGenerator::TSGenerator()
{
}

void TSGenerator::generate_still_image_ts(const char* image_path, int bitrate, bool overlay_timestamp, int duration_sec)
{
    printf("TSGenerator::generate_still_image_ts: Generating TS from image %s at bitrate %d bps for %d seconds\n",
           image_path, bitrate, duration_sec);
    ts_buffer.clear();

    // 1. Setup (one-time)
    AVFrame* frame = load_image_to_yuv_with_opencv(image_path, 1280, 720, overlay_timestamp);

    // 1. SELECT CODEC
    auto [codec, ctx] = create_codec_context(25, bitrate, 1280, 720, duration_sec);

    if (!codec || !ctx)
        return;

    avcodec_open2(ctx, codec, nullptr);

    // 2. In-memory TS context
    AVFormatContext* oc = nullptr;
    int stream_idx = setup_ts_context(&oc, ctx);
    avformat_write_header(oc, nullptr);

    // 3. Generate fixed duration TS packets
    int64_t pts = 0;
    int64_t total_frames = 25 * duration_sec;  // 25fps

    for (int64_t i = 0; i < total_frames; i++)
    {
        frame->pts = pts++;
        encode_frame_to_ts(oc, ctx, frame, stream_idx);
    }

    av_write_trailer(oc); // Finalize TS (PAT/PMT)

    // ts_buffer now contains complete, seekable TS packets
    printf("TSGenerator::generate_still_image_ts: Generated %zu bytes TS buffer\n", ts_buffer.size());
    buffer_size = ts_buffer.size();

    // 4. Cleanup
    if (frame) av_frame_free(&frame);
    if (ctx) avcodec_free_context(&ctx);
    if (oc) avformat_free_context(oc);
}

AVFrame* TSGenerator::load_image_to_yuv(const char* filename, int width, int height)
{
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* yuv_frame = nullptr;
    struct SwsContext* sws_ctx = nullptr;
    AVPacket pkt;
    int stream_idx = -1;
    AVStream* stream = nullptr;
    const AVCodec* codec = nullptr;

    // NOW all gotos are safe - no declarations bypassed
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) < 0) {
        fprintf(stderr, "TSGenerator::load_image_to_yuv Could not open image %s\n", filename);
        return nullptr;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        goto cleanup;
    }

    stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (stream_idx < 0) goto cleanup;

    stream = fmt_ctx->streams[stream_idx];
    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) goto cleanup;

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx || avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0) {
        goto cleanup;
    }

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) goto cleanup;

    // Decode single frame
    av_init_packet(&pkt);
    if (av_read_frame(fmt_ctx, &pkt) < 0) goto cleanup;

    frame = av_frame_alloc();
    if (frame && avcodec_send_packet(codec_ctx, &pkt) >= 0) {
        avcodec_receive_frame(codec_ctx, frame);
    }
    av_packet_unref(&pkt);

    if (!frame) goto cleanup;

    // Convert to YUV420P
    yuv_frame = av_frame_alloc();
    if (!yuv_frame) goto cleanup;

    yuv_frame->format = AV_PIX_FMT_YUV420P;
    yuv_frame->width = width ? width : frame->width;
    yuv_frame->height = height ? height : frame->height;
    if (av_frame_get_buffer(yuv_frame, 32) < 0) goto cleanup;

    sws_ctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format,
                            yuv_frame->width, yuv_frame->height, AV_PIX_FMT_YUV420P,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (sws_ctx) {
        sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                  yuv_frame->data, yuv_frame->linesize);
        sws_freeContext(sws_ctx);
    }

cleanup:
    if (frame) av_frame_free(&frame);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);

    return yuv_frame;
}


AVFrame* TSGenerator::load_image_to_yuv_with_opencv(const char* filename, int width, int height, bool overlay_timestamp) {
    // 1. Load image with OpenCV (simpler, supports text overlay)
    cv::Mat rgb_image = cv::imread(filename);
    if (rgb_image.empty()) {
        fprintf(stderr, "TSGenerator::load_image_to_yuv_with_opencv Failed to load image: %s\n", filename);
        return nullptr;
    }

    // 2. OPTIONAL: Overlay timestamp
    if (overlay_timestamp)
    {
        QString time_str = QDateTime::currentDateTime().toString("HH:mm:ss");
        char time_cstr[32];
        strncpy(time_cstr, time_str.toStdString().c_str(), sizeof(time_cstr) - 1);
        time_cstr[sizeof(time_cstr) - 1] = '\0';

        // Draw black background box first
        cv::rectangle(rgb_image, cv::Point(15, 28), cv::Point(170, 65),
                      cv::Scalar(0, 0, 0), -1);

        // Draw white timestamp text
        cv::putText(rgb_image, time_cstr, cv::Point(20, 55),
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    }

    // 3. BGR → RGB24
    cv::Mat rgb24;
    cv::cvtColor(rgb_image, rgb24, cv::COLOR_BGR2RGB);

    // 4. Create FFmpeg RGB frame
    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = rgb24.cols;
    rgb_frame->height = rgb24.rows;
    if (av_frame_get_buffer(rgb_frame, 32) < 0) {
        av_frame_free(&rgb_frame);
        return nullptr;
    }

    // FIXED pixel copy with proper alignment
    int linesize = rgb_frame->linesize[0];  // FFmpeg padded linesize
    int src_width_bytes = rgb24.cols * 3;   // OpenCV tight-packed

    for (int y = 0; y < rgb24.rows; y++) {
        uint8_t* dst_line = rgb_frame->data[0] + y * linesize;
        const uint8_t* src_line = rgb24.data + y * rgb24.step;

        // Copy exactly src_width_bytes, pad rest with black
        memcpy(dst_line, src_line, src_width_bytes);
        memset(dst_line + src_width_bytes, 0, linesize - src_width_bytes);
    }

    // 5. Convert RGB24 → YUV420P
    AVFrame* yuv_frame = av_frame_alloc();
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    yuv_frame->width = width ? width : rgb_frame->width;
    yuv_frame->height = height ? height : rgb_frame->height;
    if (av_frame_get_buffer(yuv_frame, 32) < 0) {
        av_frame_free(&rgb_frame);
        av_frame_free(&yuv_frame);
        return nullptr;
    }

    SwsContext* sws_ctx = sws_getContext(rgb_frame->width, rgb_frame->height, AV_PIX_FMT_RGB24,
                                        yuv_frame->width, yuv_frame->height, AV_PIX_FMT_YUV420P,
                                        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (sws_ctx) {
        sws_scale(sws_ctx, rgb_frame->data, rgb_frame->linesize, 0, rgb_frame->height,
                  yuv_frame->data, yuv_frame->linesize);
        sws_freeContext(sws_ctx);
    }

    av_frame_free(&rgb_frame);
    return yuv_frame;
}

AVCodecContext* TSGenerator::configure_hevc_context(const AVCodec* codec, int fps, int bitrate, int width, int height, int duration_sec)
{
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) return nullptr;

    // Basic video parameters
    ctx->width = width;
    ctx->height = height;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;  // Required for HEVC
    ctx->time_base = {1, fps};          // 1/fps
    ctx->framerate = {fps, 1};          // fps/1

    // Encoding parameters (matching your CLI)
    ctx->bit_rate = bitrate;            // 1000k = 1Mbps
    ctx->gop_size = 25 * duration_sec;
    ctx->max_b_frames = 0;              // Low latency (no B-frames)
    ctx->rc_buffer_size = bitrate * 2;  // Buffer size

    // HEVC-specific tuning
    av_opt_set(ctx->priv_data, "preset", "fast", 0);
    av_opt_set(ctx->priv_data, "tune", "zerolatency", 0);

    return ctx;
}

AVCodecContext* TSGenerator::configure_h264_context(const AVCodec* codec, int fps, int bitrate, int width, int height, int duration_sec)
{
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) return nullptr;

    // Basic video parameters
    ctx->width = width;
    ctx->height = height;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;  // Required for H.264
    ctx->time_base = {1, fps};          // 1/fps
    ctx->framerate = {fps, 1};          // fps/1

    // Encoding parameters
    ctx->bit_rate = bitrate;
    ctx->gop_size = 25 * duration_sec;
    ctx->max_b_frames = 0;              // Low latency
    ctx->rc_buffer_size = bitrate * 2;

    // H.264-specific tuning
    av_opt_set(ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(ctx->priv_data, "tune", "zerolatency", 0);

    return ctx;
}

int TSGenerator::setup_ts_context(AVFormatContext** oc, AVCodecContext* codec_ctx)
{
    // 1. Allocate MPEG-TS format context
    if (avformat_alloc_output_context2(oc, nullptr, "mpegts", nullptr) < 0) {
        return -1;
    }

    // Set TS metadata (optional)
    av_dict_set(&(*oc)->metadata, "service_provider", service_provider.c_str(), 0);
    av_dict_set(&(*oc)->metadata, "service_name", service_name.c_str(), 0);

    // 2. Create in-memory buffer for TS packets
    unsigned char* iobuf = (unsigned char*)av_mallocz(32768);
    if (!iobuf) return -1;

    AVIOContext* avio_ctx = avio_alloc_context(
        iobuf, 32768,  // Buffer size
        1,             // Write mode
        &ts_buffer,    // Your std::vector<uint8_t>*
        read_packet_cb, // Read callback (unused)
        write_packet_cb, // Write callback → your memory buffer
        seek_cb         // Seek callback (unused)
    );

    if (!avio_ctx) {
        av_free(iobuf);
        return -1;
    }

    (*oc)->pb = avio_ctx;

    // 3. Add video stream
    AVStream* stream = avformat_new_stream(*oc, nullptr);
    if (!stream) return -1;

    stream->id = (*oc)->nb_streams - 1;
    stream->time_base = codec_ctx->time_base;

    // Copy codec parameters to stream
    if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0) {
        return -1;
    }

    return stream->id;
}

void TSGenerator::encode_frame_to_ts(AVFormatContext* oc, AVCodecContext* codec_ctx, AVFrame* frame, int stream_idx)
{
    AVPacket pkt = {nullptr};
    int ret;

    // 1. Send frame to HEVC encoder
    ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "TSGenerator::encode_frame_to_ts Error sending frame to encoder\n");
        return;
    }

    // 2. Receive encoded packets (may produce multiple per frame)
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;  // No more packets ready
        } else if (ret < 0) {
            fprintf(stderr, "TSGenerator::encode_frame_to_ts Error receiving packet from encoder\n");
            break;
        }

        // 3. Rescale PTS to stream timebase
        pkt.stream_index = stream_idx;
        av_packet_rescale_ts(&pkt, codec_ctx->time_base, oc->streams[stream_idx]->time_base);

        // 4. Write TS packet to your memory buffer
        ret = av_interleaved_write_frame(oc, &pkt);
        if (ret < 0) {
            fprintf(stderr, "TSGenerator::encode_frame_to_ts Error writing frame to TS\n");
        }

        av_packet_unref(&pkt);
    }
}

// Write callback - stores TS packets in memory
#if LIBAVFORMAT_VERSION_INT < ((61 << 16) | (0 << 8) | 0)
int TSGenerator::write_packet_cb(void* opaque, uint8_t* buf, int buf_size)
#else
int TSGenerator::write_packet_cb(void* opaque, const uint8_t* buf, int buf_size)
#endif
{
    std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(opaque);
    buffer->insert(buffer->end(), buf, buf + buf_size);
    return buf_size;
}

int TSGenerator::read_packet_cb(void* opaque, unsigned char* buf, int buf_size)
{
    (void) opaque;
    (void) buf;
    (void) buf_size;
    return AVERROR_EOF;
}

int64_t TSGenerator::seek_cb(void* opaque, int64_t offset, int whence)
{
    (void) opaque;
    (void) offset;
    (void) whence;
    return -1;
}

double TSGenerator::get_dvbs2_rate(double symbol_rate, DATVModSettings::DATVModulation modulation, DATVModSettings::DATVCodeRate code_rate)
{
    const double (*rate_table)[4] = nullptr;
    int table_size = 0;
    double fec_num, fec_den, bits;

    switch(code_rate)
    {
        case DATVModSettings::FEC12:
            fec_num = 1.0;
            fec_den = 2.0;
            break;
        case DATVModSettings::FEC23:
            fec_num = 2.0;
            fec_den = 3.0;
            break;
        case DATVModSettings::FEC34:
            fec_num = 3.0;
            fec_den = 4.0;
            break;
        case DATVModSettings::FEC45:
            fec_num = 4.0;
            fec_den = 5.0;
            break;
        case DATVModSettings::FEC56:
            fec_num = 5.0;
            fec_den = 6.0;
            break;
        case DATVModSettings::FEC78:
            fec_num = 7.0;
            fec_den = 8.0;
            break;
        case DATVModSettings::FEC89:
            fec_num = 8.0;
            fec_den = 9.0;
            break;
        case DATVModSettings::FEC910:
            fec_num = 9.0;
            fec_den = 10.0;
            break;
        case DATVModSettings::FEC14:
            fec_num = 1.0;
            fec_den = 4.0;
            break;
        case DATVModSettings::FEC13:
            fec_num = 1.0;
            fec_den = 3.0;
            break;
        case DATVModSettings::FEC25:
            fec_num = 2.0;
            fec_den = 5.0;
            break;
        case DATVModSettings::FEC35:
            fec_num = 3.0;
            fec_den = 5.0;
            break;
        default:
            return symbol_rate * (fec_num / fec_den); // others
    }

    switch (modulation) {
        case DATVModSettings::QPSK:
            bits = 2.0;
            rate_table = TSGenerator::rate_qpsk;
            table_size = 11;
            break;
        case DATVModSettings::PSK8:
            bits = 3.0;
            rate_table = TSGenerator::rate_8psk;
            table_size = 6;
            break;
        case DATVModSettings::APSK16:
            bits = 4.0;
            rate_table = TSGenerator::rate_16apsk;
            table_size = 6;
            break;
        case DATVModSettings::APSK32:
            bits = 5.0;
            rate_table = TSGenerator::rate_32apsk;
            table_size = 5;
            break;
        default:
            return symbol_rate * (fec_num / fec_den); // BPSK and others
    }

    for (int i = 0; i < table_size; i++) {
        if (rate_table[i][0] == fec_num && rate_table[i][1] == fec_den) {
            return TSGenerator::calc_dvbs2_rate(symbol_rate, bits, fec_num, fec_den, rate_table[i][2], 0.0);
        }
    }

    return symbol_rate * (fec_num / fec_den); // others
}

double TSGenerator::calc_dvbs2_rate(double symbol_rate, double bits, double fec_num, double fec_den, double bch, double pilots)
{
    double    fec_frame = 64800.0;
    double    tsrate;

    tsrate = symbol_rate / (fec_frame / bits + 90 + ceil(fec_frame/ bits / 90 / 16 - 1) * pilots) * (fec_frame * (fec_num / fec_den) - (16 * bch) - 80);
    return (tsrate);
}

uint8_t *TSGenerator::next_ts_packet()
{
    static size_t read_pos = 0;
    const size_t ts_packet_size = 188;

    if (read_pos + ts_packet_size > buffer_size) {
        read_pos = 0; // Loop back to start
    }

    uint8_t* packet = ts_buffer.data() + read_pos;
    read_pos += ts_packet_size;

    if (packet[0] != 0x47) { // no sync byte
        return packet;
    }

    // PATCH continuity counter (byte 3, bits 0-3)
    uint16_t pid = ((packet[1] & 0x1F) << 8) | packet[2];
    uint8_t& cc = continuity_counters[pid];

    // Clear old CC, set new CC
    packet[3] = (packet[3] & 0xF0) | cc;

    // Increment CC (rolls over 0-15)
    cc = (cc + 1) & 0x0F;

    return packet;
}

std::pair<const AVCodec*, AVCodecContext*> TSGenerator::create_codec_context(int fps, int bitrate, int width, int height, int duration_sec)
{
    const AVCodec* codec = nullptr;
    AVCodecContext* ctx = nullptr;

    switch (m_codecType)
    {
        case DATVModSettings::CodecH264:
            codec = avcodec_find_encoder_by_name("libx264");
            ctx = configure_h264_context(codec, fps, bitrate, width, height, duration_sec);
            break;
        case DATVModSettings::CodecHEVC:
        default:
            codec = avcodec_find_encoder_by_name("libx265");
            ctx = configure_hevc_context(codec, fps, bitrate, width, height, duration_sec);
            break;
     }

    if (!codec || !ctx) {
        fprintf(stderr, "TSGenerator::create_codec_context: Codec not available\n");
        return {nullptr, nullptr};
    }

    return {codec, ctx};
}
