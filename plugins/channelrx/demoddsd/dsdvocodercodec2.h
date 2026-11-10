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

#ifndef INCLUDE_DSDVOCODERCODEC2_H
#define INCLUDE_DSDVOCODERCODEC2_H

#include "dsdprotocol.h"

struct CODEC2;

/**
 * Codec2 3200 decoder for M17: a frame of 8 bytes to 160 samples at 8 kHz. The codec is linked
 * in and deterministic, so there is nothing to load and no erasure concealment beyond what the
 * codec does with a repeated frame; a lost frame is decoded as silence and the stream carries on.
 * One instance per audio stream.
 */
class DSDVocoderCodec2
{
public:
    static const int m_samplesPerFrame = 160;
    static const int m_sampleRate = 8000;
    static const int m_frameBytes = 8;

    DSDVocoderCodec2();
    ~DSDVocoderCodec2();
    DSDVocoderCodec2(const DSDVocoderCodec2&) = delete;
    DSDVocoderCodec2& operator=(const DSDVocoderCodec2&) = delete;

    //! A new transmission
    void reset();
    //! One frame to 160 samples (float, full scale +-32767). False when the codec could not open.
    bool decode(const DSDVoiceFrame& frame, float *audio);
    int getLostFrames() const { return m_lostFrames; }

private:
    struct CODEC2 *m_codec2;
    int m_lostFrames;

    bool open();
};

#endif // INCLUDE_DSDVOCODERCODEC2_H
