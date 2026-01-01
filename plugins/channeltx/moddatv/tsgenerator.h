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

#ifndef INCLUDE_ATVMOD_TSGENERATOR_H
#define INCLUDE_ATVMOD_TSGENERATOR_H

#include <vector>
#include <cstdint>
#include <string>

#include "datvmodsettings.h"

struct AVFrame;
struct AVFormatContext;
struct AVCodecContext;
struct AVCodec;

class TSGenerator
{
public:
    enum class CodecType {
        HEVC,   // libx265
        H264,   // libx264
    };

    TSGenerator();
    ~TSGenerator() = default;
    void generate_still_image_ts(const char* image_path, int bitrate, bool overlay_timestamp, int duration_sec = 1);
    void set_service_provider(const std::string& provider) { service_provider = provider; }
    void set_service_name(const std::string& name) { service_name = name; }
    uint8_t *next_ts_packet();
    int get_buffer_size() const { return static_cast<int>(buffer_size); }
    void set_codec(DATVModSettings::DATVCodec codec) { m_codecType = codec; }

private:
    std::vector<uint8_t> ts_buffer;  // Complete TS packets (e.g., 10s worth)
    size_t buffer_size = 0;
    size_t write_pos = 0;
    std::string service_provider = "SDRangel";
    std::string service_name = "SDRangel_TV";
    bool m_generateImage = false;
    DATVModSettings::DATVCodec m_codecType = DATVModSettings::CodecHEVC;
    static const double    rate_qpsk[11][4];
    static const double    rate_8psk[6][4];
    static const double    rate_16apsk[6][4];
    static const double    rate_32apsk[5][4];
    static uint8_t continuity_counters[8192];  // One per PID (0-8191)

    AVFrame* load_image_to_yuv(const char* filename, int width, int height);
    AVFrame* load_image_to_yuv_with_opencv(const char* filename, int width, int height, bool overlay_timestamp = false);
    AVCodecContext* configure_hevc_context(const AVCodec* codec, int fps, int bitrate, int width, int height, int duration_sec);
    AVCodecContext* configure_h264_context(const AVCodec* codec, int fps, int bitrate, int width, int height, int duration_sec);
    int setup_ts_context(AVFormatContext** oc, AVCodecContext* codec_ctx);
    void encode_frame_to_ts(AVFormatContext* oc, AVCodecContext* codec_ctx, AVFrame* frame, int stream_idx = 0);
    std::pair<const AVCodec*, AVCodecContext*> create_codec_context(int fps, int bitrate, int width, int height, int duration_sec);
    static int write_packet_cb(void* opaque, uint8_t* buf, int buf_size);
    static int read_packet_cb(void* opaque, uint8_t* buf, int buf_size);
    static int64_t seek_cb(void* opaque, int64_t offset, int whence);
    static double get_dvbs2_rate(double symbol_rate, DATVModSettings::DATVModulation modulation, DATVModSettings::DATVCodeRate code_rate);
    static double calc_dvbs2_rate(double symbol_rate, double bits, double fec_num, double fec_den, double bch, double pilots);
};

#endif // INCLUDE_ATVMOD_TSGENERATOR_H
