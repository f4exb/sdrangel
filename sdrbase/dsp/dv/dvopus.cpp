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

#include "opus/opus.h"

#include "dvopus.h"

std::string DVOpus::getVersion()
{
    return opus_get_version_string();
}

std::string DVOpus::describePacket(const uint8_t *packet, int nbBytes)
{
    if (nbBytes < 1) {
        return "empty";
    }

    int config = packet[0] >> 3;
    const char *mode = config < 12 ? "SILK" : config < 16 ? "hybrid" : "CELT";
    const char *bandwidth = "?";

    switch (opus_packet_get_bandwidth(packet))
    {
    case OPUS_BANDWIDTH_NARROWBAND: bandwidth = "NB"; break;
    case OPUS_BANDWIDTH_MEDIUMBAND: bandwidth = "MB"; break;
    case OPUS_BANDWIDTH_WIDEBAND: bandwidth = "WB"; break;
    case OPUS_BANDWIDTH_SUPERWIDEBAND: bandwidth = "SWB"; break;
    case OPUS_BANDWIDTH_FULLBAND: bandwidth = "FB"; break;
    default: break;
    }

    int frames = opus_packet_get_nb_frames(packet, nbBytes);
    int frameMs = opus_packet_get_samples_per_frame(packet, SampleRate) / (SampleRate / 1000);
    return std::string(mode) + " " + bandwidth + " " + std::to_string(frames) + "x" + std::to_string(frameMs) + " ms";
}

DVOpus::Encoder::Encoder() :
    m_encoder(nullptr),
    m_frameSamples(FrameSamples),
    m_packetBytes(PacketBytes)
{
}

DVOpus::Encoder::~Encoder()
{
    close();
}

bool DVOpus::Encoder::open(int frameSamples, int packetBytes)
{
    close();
    m_frameSamples = frameSamples;
    m_packetBytes = packetBytes;
    int error = 0;
    m_encoder = opus_encoder_create(SampleRate, 1, OPUS_APPLICATION_AUDIO, &error);

    if ((error != OPUS_OK) || !m_encoder)
    {
        m_encoder = nullptr;
        return false;
    }

    // Constant bit rate: every packet exactly its bytes (184: 18.4 kbit/s). No in band FEC: its
    // copy of a frame would cost bits of the packet and a frame of playout delay to use (ANGEL61.md
    // 3.1.2, section 10)
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE((opus_int32) ((int64_t) m_packetBytes * 8 * SampleRate / m_frameSamples)));
    opus_encoder_ctl(m_encoder, OPUS_SET_VBR(0));
    opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(0));
    opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(0));
    opus_encoder_ctl(m_encoder, OPUS_SET_DTX(0));
    return true;
}

void DVOpus::Encoder::close()
{
    if (m_encoder)
    {
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
    }
}

bool DVOpus::Encoder::encode(const int16_t *pcm, uint8_t *packet)
{
    if (!m_encoder) {
        return false;
    }

    opus_int32 bytes = opus_encode(m_encoder, pcm, m_frameSamples, packet, m_packetBytes);

    if (bytes <= 0) {
        return false;
    }

    // CBR fills the packet; should a build ever return less, the rest is padding the decoder is
    // given with it (a packet's length is the frame's, never its own)
    for (int i = bytes; i < m_packetBytes; i++) {
        packet[i] = 0;
    }

    return bytes == m_packetBytes;
}

DVOpus::Decoder::Decoder() :
    m_decoder(nullptr),
    m_frameSamples(FrameSamples),
    m_packetBytes(PacketBytes),
    m_complexity(DefaultComplexity),
    m_bandwidthExtension(false)
{
}

DVOpus::Decoder::~Decoder()
{
    close();
}

bool DVOpus::Decoder::open(int frameSamples, int packetBytes)
{
    close();
    m_frameSamples = frameSamples;
    m_packetBytes = packetBytes;
    int error = 0;
    m_decoder = opus_decoder_create(SampleRate, 1, &error);

    if ((error != OPUS_OK) || !m_decoder)
    {
        m_decoder = nullptr;
        return false;
    }

    applyEnhancements();
    return true;
}

void DVOpus::Decoder::setEnhancements(int complexity, bool bandwidthExtension)
{
    m_complexity = complexity;
    m_bandwidthExtension = bandwidthExtension;
    applyEnhancements();
}

void DVOpus::Decoder::applyEnhancements()
{
    if (!m_decoder) {
        return;
    }

    // A library before 1.5 answers OPUS_UNIMPLEMENTED, one without OSCE ignores the complexity
    opus_decoder_ctl(m_decoder, OPUS_SET_COMPLEXITY(m_complexity));
#ifdef OPUS_SET_OSCE_BWE_REQUEST
    opus_decoder_ctl(m_decoder, OPUS_SET_OSCE_BWE(m_bandwidthExtension ? 1 : 0));
#endif
}

void DVOpus::Decoder::close()
{
    if (m_decoder)
    {
        opus_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }
}

bool DVOpus::Decoder::decode(const uint8_t *packet, int16_t *pcm)
{
    return m_decoder && (opus_decode(m_decoder, packet, m_packetBytes, pcm, m_frameSamples, 0) == m_frameSamples);
}

bool DVOpus::Decoder::decodeLost(int16_t *pcm)
{
    return m_decoder && (opus_decode(m_decoder, nullptr, 0, pcm, m_frameSamples, 0) == m_frameSamples);
}

void DVOpus::Decoder::reset()
{
    if (m_decoder) {
        opus_decoder_ctl(m_decoder, OPUS_RESET_STATE);
    }
}
