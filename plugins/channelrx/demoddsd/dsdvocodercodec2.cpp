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

#include <cstring>

#include "codec2/codec2.h"

#include "dsdvocodercodec2.h"

DSDVocoderCodec2::DSDVocoderCodec2() :
    m_codec2(nullptr),
    m_lostFrames(0)
{
}

DSDVocoderCodec2::~DSDVocoderCodec2()
{
    if (m_codec2) {
        codec2_destroy(m_codec2);
    }
}

bool DSDVocoderCodec2::open()
{
    if (!m_codec2) {
        m_codec2 = codec2_create(CODEC2_MODE_3200);
    }

    return m_codec2 != nullptr;
}

void DSDVocoderCodec2::reset()
{
    if (m_codec2)
    {
        codec2_destroy(m_codec2);
        m_codec2 = nullptr;
    }

    m_lostFrames = 0;
}

bool DSDVocoderCodec2::decode(const DSDVoiceFrame& frame, float *audio)
{
    if (!open()) {
        return false;
    }

    if (frame.m_erasure)
    {
        // Codec2 has no concealment call: the 20 ms are silence and the stream keeps its time
        m_lostFrames++;
        std::fill(audio, audio + m_samplesPerFrame, 0.0f);
        return true;
    }

    int16_t samples[m_samplesPerFrame];
    codec2_decode(m_codec2, samples, const_cast<uint8_t*>(frame.m_bytes));

    for (int i = 0; i < m_samplesPerFrame; i++) {
        audio[i] = (float) samples[i];
    }

    return true;
}
