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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODEROPUS_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODEROPUS_H_

#include "dsp/dv/dvopus.h"

#include "dsemodvocoder.h"

/**
 * Opus encoder back end for Angel61's codec family O1 (sdrbase/dsp/dv/ANGEL61.md 3.1.2), WB's tier 3:
 * audio at 48 kHz in 20 ms frames of 960 samples, as every other rate is fed, four of them to one
 * 80 ms packet of 184 bytes, delivered with the fourth. The protocol takes one packet per 80 ms
 * frame, so the pacing is the protocol's frames, as for the other rates. No models, nothing loaded
 * at run time: libopus is part of sdrbase (DVOpus).
 */
class DSEModVocoderOpus : public DSEModVocoder
{
public:
    DSEModVocoderOpus();
    virtual ~DSEModVocoderOpus();

    virtual bool open(Rate rate);
    virtual void close();
    virtual bool isReady() const { return m_encoder.isOpen(); }
    virtual bool canOpen() const { return true; }
    virtual bool canEncode(Rate rate) const { return isOpus(rate); }
    virtual void encode(uint32_t sequence, const int16_t *pcm);
    virtual int getLatencyFrames() const { return 0; }
    virtual const char *getName() const { return "opus"; }
    //! The partial packet dropped (key down)
    void reset() { m_filled = 0; }

private:
    DVOpus::Encoder m_encoder;
    int16_t m_pcm[DVOpus::FrameSamples];
    int m_filled;               //!< Samples collected towards the next packet
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODEROPUS_H_
