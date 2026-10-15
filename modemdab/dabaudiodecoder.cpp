///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>
#include <cmath>

#include "dabprofile.h"
#include "dabaudiodecoder.h"

#ifdef MODEMDAB_HAS_FAAD2
#include <neaacdec.h>
#endif

#ifdef MODEMDAB_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}
#endif

namespace modemdab
{

namespace
{

int16_t clip16(float v)
{
    v = std::round(v * 32768.0f);
    return static_cast<int16_t>(std::max(-32768.0f, std::min(32767.0f, v)));
}

#ifdef MODEMDAB_HAS_FAAD2

// HE-AAC v2 with the 960 sample transform (TS 102 563)
class FAAD2Decoder : public DABAudioDecoder
{
public:
    explicit FAAD2Decoder(const DABPlusParameters& parameters) :
        m_parameters(parameters),
        m_handle(nullptr)
    {
        open();
    }

    ~FAAD2Decoder() override
    {
        if (m_handle) {
            NeAACDecClose(m_handle);
        }
    }

    bool valid() const { return m_handle != nullptr; }
    std::string backend() const override { return "FAAD2"; }

    void reset() override
    {
        if (m_handle) {
            NeAACDecClose(m_handle);
        }
        open();
    }

protected:
    bool decodeFrame(const uint8_t *data, int size, DABPCM& pcm) override
    {
        if (!m_handle || (size <= 0)) {
            return false;
        }

        NeAACDecFrameInfo info;
        void *output = NeAACDecDecode(m_handle, &info, const_cast<unsigned char *>(data), static_cast<unsigned long>(size));

        if (info.error || !output || (info.channels < 1)) {
            return false;
        }
        if (info.samples == 0)
        {
            // The decoder's first frame (priming) has no output
            pcm.samples.clear();
            return true;
        }

        const int channels = info.channels;
        const int frames = static_cast<int>(info.samples / channels);
        const int16_t *in = static_cast<const int16_t *>(output);

        pcm.samples.resize(2 * frames);
        for (int i = 0; i < frames; i++)
        {
            pcm.samples[2 * i] = in[i * channels];
            pcm.samples[2 * i + 1] = in[i * channels + (channels > 1 ? 1 : 0)];
        }
        pcm.sampleRate = static_cast<int>(info.samplerate);
        return true;
    }

    int frameLength() const override { return m_parameters.sbr ? 1920 : 960; }
    int sampleRate() const override { return m_parameters.outputSampleRate(); }

private:
    DABPlusParameters m_parameters;
    NeAACDecHandle m_handle;

    void open()
    {
        m_handle = NeAACDecOpen();
        if (!m_handle) {
            return;
        }

        NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(m_handle);
        config->outputFormat = FAAD_FMT_16BIT;
        config->dontUpSampleImplicitSBR = 0;
        NeAACDecSetConfiguration(m_handle, config);

        // AudioSpecificConfig: AAC LC, the core sampling rate, the channel configuration and
        // GASpecificConfig with frameLengthFlag set (960 transform). SBR and PS are signalled
        // implicitly: FAAD2 assumes SBR for core rates up to 24 kHz.
        int rateIndex;
        switch (m_parameters.coreSampleRate())
        {
        case 48000: rateIndex = 3; break;
        case 32000: rateIndex = 5; break;
        case 24000: rateIndex = 6; break;
        default: rateIndex = 8; break;  // 16000
        }
        const int channelConfiguration = m_parameters.stereo ? 2 : 1;
        const unsigned asc = (2u << 11) | (static_cast<unsigned>(rateIndex) << 7) | (static_cast<unsigned>(channelConfiguration) << 3) | (1u << 2);
        unsigned char config2[2] = {static_cast<unsigned char>(asc >> 8), static_cast<unsigned char>(asc & 0xff)};
        unsigned long rate;
        unsigned char channels;

        if (NeAACDecInit2(m_handle, config2, 2, &rate, &channels) < 0)
        {
            NeAACDecClose(m_handle);
            m_handle = nullptr;
        }
    }
};

#endif // MODEMDAB_HAS_FAAD2

#ifdef MODEMDAB_HAS_FFMPEG

// MPEG-1/2 Layer II
class FFmpegMP2Decoder : public DABAudioDecoder
{
public:
    FFmpegMP2Decoder() :
        m_context(nullptr),
        m_packet(nullptr),
        m_frame(nullptr),
        m_sampleRate(48000)
    {
        open();
    }

    ~FFmpegMP2Decoder() override
    {
        close();
    }

    bool valid() const { return m_context != nullptr; }
    std::string backend() const override { return "FFmpeg"; }

    void reset() override
    {
        close();
        open();
    }

protected:
    bool decodeFrame(const uint8_t *data, int size, DABPCM& pcm) override
    {
        if (!m_context) {
            return false;
        }

        m_packet->data = const_cast<uint8_t *>(data);
        m_packet->size = size;
        if (avcodec_send_packet(m_context, m_packet) < 0) {
            return false;
        }

        pcm.samples.clear();
        while (avcodec_receive_frame(m_context, m_frame) == 0)
        {
#if LIBAVUTIL_VERSION_MAJOR >= 57
            const int channels = m_frame->ch_layout.nb_channels;
#else
            const int channels = m_frame->channels;
#endif
            const int frames = m_frame->nb_samples;
            const int format = m_frame->format;
            const std::size_t offset = pcm.samples.size();

            pcm.samples.resize(offset + 2 * frames);
            for (int i = 0; i < frames; i++)
            {
                for (int ch = 0; ch < 2; ch++)
                {
                    const int c = std::min(ch, channels - 1);
                    int16_t v = 0;
                    switch (format)
                    {
                    case AV_SAMPLE_FMT_S16:
                        v = reinterpret_cast<const int16_t *>(m_frame->data[0])[i * channels + c];
                        break;
                    case AV_SAMPLE_FMT_S16P:
                        v = reinterpret_cast<const int16_t *>(m_frame->data[c])[i];
                        break;
                    case AV_SAMPLE_FMT_FLT:
                        v = clip16(reinterpret_cast<const float *>(m_frame->data[0])[i * channels + c]);
                        break;
                    case AV_SAMPLE_FMT_FLTP:
                        v = clip16(reinterpret_cast<const float *>(m_frame->data[c])[i]);
                        break;
                    default:
                        break;
                    }
                    pcm.samples[offset + 2 * i + ch] = v;
                }
            }
            pcm.sampleRate = m_frame->sample_rate;
            m_sampleRate = m_frame->sample_rate;
            av_frame_unref(m_frame);
        }
        return !pcm.samples.empty();
    }

    int frameLength() const override { return 1152; }
    int sampleRate() const override { return m_sampleRate; }

private:
    AVCodecContext *m_context;
    AVPacket *m_packet;
    AVFrame *m_frame;
    int m_sampleRate;

    void open()
    {
        const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MP2);
        if (!codec) {
            return;
        }
        m_context = avcodec_alloc_context3(codec);
        m_packet = av_packet_alloc();
        m_frame = av_frame_alloc();
        if (!m_context || !m_packet || !m_frame || (avcodec_open2(m_context, codec, nullptr) < 0)) {
            close();
        }
    }

    void close()
    {
        if (m_context) {
            avcodec_free_context(&m_context);
        }
        if (m_packet) {
            av_packet_free(&m_packet);
        }
        if (m_frame) {
            av_frame_free(&m_frame);
        }
    }
};

#endif // MODEMDAB_HAS_FFMPEG

} // anonymous namespace

bool DABAudioDecoder::decode(const uint8_t *data, int size, DABPCM& pcm)
{
    DABProfileScope profile(DABProfile::AudioDecoding);
    pcm.concealed = false;
    if (!decodeFrame(data, size, pcm))
    {
        conceal(pcm);
        return false;
    }
    if (pcm.samples.empty()) {
        return true;    // Nothing output yet
    }

    // Fade in after concealment
    if (m_concealed > 0)
    {
        const int frames = pcm.frames();
        for (int i = 0; i < frames; i++)
        {
            const float gain = static_cast<float>(i) / frames;
            pcm.samples[2 * i] = static_cast<int16_t>(pcm.samples[2 * i] * gain);
            pcm.samples[2 * i + 1] = static_cast<int16_t>(pcm.samples[2 * i + 1] * gain);
        }
    }
    m_concealed = 0;
    m_last = pcm.samples;
    m_lastRate = pcm.sampleRate;
    return true;
}

void DABAudioDecoder::conceal(DABPCM& pcm)
{
    pcm.concealed = true;
    pcm.sampleRate = m_lastRate > 0 ? m_lastRate : sampleRate();

    if ((m_concealed == 0) && !m_last.empty())
    {
        // The last frame again, faded out
        pcm.samples = m_last;
        const int frames = pcm.frames();
        for (int i = 0; i < frames; i++)
        {
            const float gain = 1.0f - static_cast<float>(i) / frames;
            pcm.samples[2 * i] = static_cast<int16_t>(pcm.samples[2 * i] * gain);
            pcm.samples[2 * i + 1] = static_cast<int16_t>(pcm.samples[2 * i + 1] * gain);
        }
    }
    else
    {
        const int frames = m_last.empty() ? frameLength() : static_cast<int>(m_last.size() / 2);
        pcm.samples.assign(2 * frames, 0);
    }
    m_concealed++;
}

bool DABAudioDecoder::aacAvailable()
{
#ifdef MODEMDAB_HAS_FAAD2
    return true;
#else
    return false;
#endif
}

bool DABAudioDecoder::mp2Available()
{
#ifdef MODEMDAB_HAS_FFMPEG
    return true;
#else
    return false;
#endif
}

std::unique_ptr<DABAudioDecoder> DABAudioDecoder::createAAC(const DABPlusParameters& parameters)
{
#ifdef MODEMDAB_HAS_FAAD2
    std::unique_ptr<FAAD2Decoder> decoder(new FAAD2Decoder(parameters));
    if (decoder->valid()) {
        return std::unique_ptr<DABAudioDecoder>(decoder.release());
    }
#else
    (void) parameters;
#endif
    return nullptr;
}

std::unique_ptr<DABAudioDecoder> DABAudioDecoder::createMP2()
{
#ifdef MODEMDAB_HAS_FFMPEG
    std::unique_ptr<FFmpegMP2Decoder> decoder(new FFmpegMP2Decoder());
    if (decoder->valid()) {
        return std::unique_ptr<DABAudioDecoder>(decoder.release());
    }
#endif
    return nullptr;
}

} // namespace modemdab
