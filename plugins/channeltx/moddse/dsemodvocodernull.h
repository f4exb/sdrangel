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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERNULL_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERNULL_H_

#include "dsemodvocoder.h"

/**
 * Vocoder back end that ignores the audio and returns a fixed silence frame.
 * Used to test the protocol layers without any AMBE encoder.
 */
class DSEModVocoderNull : public DSEModVocoder
{
public:
    DSEModVocoderNull();
    virtual ~DSEModVocoderNull();

    virtual bool open(Rate rate);
    virtual void close();
    virtual bool canOpen() const { return false; }
    virtual bool isReady() const { return m_rate != RateNone; }
    virtual void encode(uint32_t sequence, const int16_t *pcm);
    virtual int getLatencyFrames() const { return 0; }
    virtual const char *getName() const { return "Null"; }
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERNULL_H_
