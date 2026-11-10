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

#include "dsemodvocodervocos.h"

DSEModVocoderVocos::DSEModVocoderVocos()
{
}

DSEModVocoderVocos::~DSEModVocoderVocos()
{
    close();
}

bool DSEModVocoderVocos::canOpen() const
{
    return VocosCodec::isAvailable() && !(m_modelDirectory.empty() ? VocosCodec::defaultModelDirectory() : m_modelDirectory).empty();
}

bool DSEModVocoderVocos::open(Rate rate)
{
    if (isVocos(rate) && m_encoder.isOpen())
    {
        // The encoder afresh, as at key down (3.1.1)
        m_encoder.reset();
        m_pending.clear();
        m_rate = rate;
        return true;
    }

    close();

    if (!isVocos(rate))
    {
        m_error = "not the V1 rate";
        return false;
    }

    if (!m_encoder.open(VocosCodec::MaxCodebooks, m_modelDirectory))
    {
        m_error = m_encoder.getError();
        return false;
    }

    m_rate = rate;
    m_error.clear();
    m_pending.clear();

    // The packet the codec makes of silence, for the packets sent when there is no audio; then the
    // encoder afresh, as at key down (3.1.1)
    std::vector<int16_t> zeros(VocosCodec::SamplesPerFrame, 0);
    uint8_t packet[VocosCodec::MaxPacketBytes];

    if (m_encoder.encode(zeros.data(), packet, VocosCodec::MaxPacketBytes) == getFrameBytes(rate)) {
        setSilenceFrame(rate, packet);
    }

    m_encoder.reset();
    return true;
}

void DSEModVocoderVocos::close()
{
    m_encoder.close();
    m_pending.clear();
    m_rate = RateNone;
}

void DSEModVocoderVocos::encode(uint32_t sequence, const int16_t *pcm)
{
    m_pending.insert(m_pending.end(), pcm, pcm + getPcmFrameSize(RateVocos));

    while (m_pending.size() >= (size_t) VocosCodec::SamplesPerFrame)
    {
        uint8_t packet[VocosCodec::MaxPacketBytes] = { 0 };

        if (!m_encoder.isOpen() || (m_encoder.encode(m_pending.data(), packet, VocosCodec::MaxPacketBytes) != getFrameBytes(RateVocos)))
        {
            m_error = m_encoder.getError();
            deliver(sequence, getSilenceFrame(RateVocos));
        }
        else
        {
            deliver(sequence, packet);
        }

        m_pending.erase(m_pending.begin(), m_pending.begin() + VocosCodec::SamplesPerFrame);
    }
}
