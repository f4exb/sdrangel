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

#include "dsdvocoderopus.h"

DSDVocoderOpus::DSDVocoderOpus() :
    m_concealedPackets(0),
    m_lostPackets(0)
{
}

void DSDVocoderOpus::reset()
{
    m_decoder.reset();
    m_concealedPackets = 0;
    m_lostPackets = 0;
}

bool DSDVocoderOpus::decode(const DSDVoiceFrame& frame, float *audio, bool *concealed)
{
    const int frameSamples = DSDVoiceFrame::getAudioSamples(frame.m_rate);
    std::fill(audio, audio + std::min(frameSamples, (int) m_maxSamplesPerFrame), 0.0f);

    if (concealed) {
        *concealed = false;
    }

    if (!DSDVoiceFrame::isOpus(frame.m_rate)) {
        return false;
    }

    if (!m_decoder.isOpen())
    {
        if (!m_decoder.open(DVOpus::FrameSamples, DVOpus::PacketBytes)) {
            return false;
        }
    }

    int16_t samples[m_maxSamplesPerFrame];
    bool ok;

    if (frame.m_erasure)
    {
        // Lost audio keeps its time (ANGEL61.md 3.1): the codec's concealment fills the 80 ms
        m_lostPackets++;
        m_concealedPackets++;

        if (concealed) {
            *concealed = true;
        }

        ok = m_decoder.decodeLost(samples);
    }
    else
    {
        m_lostPackets = 0;
        ok = m_decoder.decode(frame.m_bytes, samples);
    }

    if (!ok) {
        return false;
    }

    for (int i = 0; i < frameSamples; i++) {
        audio[i] = (float) samples[i];
    }

    return true;
}
