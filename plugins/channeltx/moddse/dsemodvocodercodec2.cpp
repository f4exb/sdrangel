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

#include <QDebug>

#include "codec2/codec2.h"

#include "dsemodvocodercodec2.h"

DSEModVocoderCodec2::DSEModVocoderCodec2() :
    m_codec2(nullptr)
{
}

DSEModVocoderCodec2::~DSEModVocoderCodec2()
{
    close();
}

bool DSEModVocoderCodec2::open(Rate rate)
{
    close();

    if (rate != RateCodec23200)
    {
        qWarning("DSEModVocoderCodec2::open: rate %d is not Codec2 3200", (int) rate);
        return false;
    }

    m_codec2 = codec2_create(CODEC2_MODE_3200);

    if (!m_codec2)
    {
        qWarning("DSEModVocoderCodec2::open: codec2_create failed");
        return false;
    }

    m_rate = rate;

    // What the encoder makes of silence, for the frames a transmission needs and has nothing for
    int16_t silence[160] = { 0 };
    uint8_t frame[8] = { 0 };
    codec2_encode(m_codec2, frame, silence);
    setSilenceFrame(rate, frame);
    return true;
}

void DSEModVocoderCodec2::close()
{
    if (m_codec2)
    {
        codec2_destroy(m_codec2);
        m_codec2 = nullptr;
    }

    m_rate = RateNone;
}

void DSEModVocoderCodec2::encode(uint32_t sequence, const int16_t *pcm)
{
    if (!m_codec2) {
        return;
    }

    uint8_t frame[m_maxFrameBytes] = { 0 };
    codec2_encode(m_codec2, frame, const_cast<int16_t*>(pcm));
    deliver(sequence, frame);
}
