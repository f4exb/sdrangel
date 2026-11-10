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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERCODEC2_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERCODEC2_H_

#include "dsemodvocoder.h"

struct CODEC2;

/**
 * Codec2 3200 encoder for M17: 160 samples at 8 kHz in, 8 bytes out, synchronously. The codec
 * is linked in (it is a dependency of this application already), deterministic and small, so
 * unlike the vocoders of the other protocols there is nothing to find at run time.
 */
class DSEModVocoderCodec2 : public DSEModVocoder
{
public:
    DSEModVocoderCodec2();
    virtual ~DSEModVocoderCodec2();

    virtual bool open(Rate rate);
    virtual void close();
    virtual bool isReady() const { return m_codec2 != nullptr; }
    virtual bool canOpen() const { return true; }
    virtual bool canEncode(Rate rate) const { return rate == RateCodec23200; }
    virtual void encode(uint32_t sequence, const int16_t *pcm);
    virtual int getLatencyFrames() const { return 0; }
    virtual const char *getName() const { return "codec2"; }

private:
    struct CODEC2 *m_codec2;
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERCODEC2_H_
