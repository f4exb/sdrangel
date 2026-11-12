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

#include "dsemodvocoderopus.h"

DSEModVocoderOpus::DSEModVocoderOpus() :
    m_filled(0)
{
}

DSEModVocoderOpus::~DSEModVocoderOpus()
{
    close();
}

bool DSEModVocoderOpus::open(Rate rate)
{
    close();

    if (!isOpus(rate) || !m_encoder.open(DVOpus::FrameSamples, DVOpus::PacketBytes)) {
        return false;
    }

    m_rate = rate;
    m_filled = 0;

    // The packet the codec makes of silence, for the frames sent when there is no audio: a
    // packet of zeros is not a valid Opus packet
    std::fill(m_pcm, m_pcm + m_encoder.getFrameSamples(), 0);
    uint8_t packet[DVOpus::PacketBytes];

    if (m_encoder.encode(m_pcm, packet)) {
        setSilenceFrame(rate, packet);
    }

    m_encoder.open(m_encoder.getFrameSamples(), m_encoder.getPacketBytes());   // afresh, so that the silence leaves no state behind
    return true;
}

void DSEModVocoderOpus::close()
{
    m_encoder.close();
    m_rate = RateNone;
    m_filled = 0;
}

void DSEModVocoderOpus::encode(uint32_t sequence, const int16_t *pcm)
{
    const int n = getPcmFrameSize(RateOpus80);
    std::copy(pcm, pcm + n, m_pcm + m_filled);
    m_filled += n;

    if (m_filled < m_encoder.getFrameSamples()) {
        return;
    }

    m_filled = 0;
    uint8_t packet[DVOpus::PacketBytes];

    if (!m_encoder.encode(m_pcm, packet))
    {
        deliver(sequence, getSilenceFrame(m_rate));
        return;
    }

    deliver(sequence, packet);
}
