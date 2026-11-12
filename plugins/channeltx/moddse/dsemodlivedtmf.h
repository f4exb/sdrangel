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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODLIVEDTMF_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODLIVEDTMF_H_

#include <cstdint>
#include <deque>
#include <vector>

#include "dsp/dv/dvdtmf.h"

/**
 * Angel61's live DTMF (sdrbase/dsp/dv/ANGEL61.md 3.8): the keys in the speech input, to be sent as
 * data, and the speech with them taken out. The speech is delayed by the detector's decision time
 * and 20 ms, and muted from 20 ms before a key's tone to 20 ms after it, so that no part of a tone
 * reaches the encoder. A key is given once its tone has ended, with its measured length and the
 * silence before it; a key 2 s or more after the last begins a new sequence. No Qt: dsemodtest
 * checks it.
 */
class DSEModLiveDTMF
{
public:
    struct Key
    {
        char m_key = 0;
        int m_lengthMs = 0;
        int m_gapMs = 100;          //!< the silence before it (100 for a sequence's first)
        bool m_newSequence = true;
    };

    DSEModLiveDTMF() { setSampleRate(24000); }
    void setSampleRate(int sampleRate);
    int getSampleRate() const { return m_detector.getSampleRate(); }
    void reset();
    //! The delay the speech goes through, in samples
    int getDelay() const { return m_detector.decisionDelay() + m_margin; }
    //! Speech in place (16 bit): delayed, its tones muted; the keys that ended appended to `keys`
    void process(int16_t *pcm, int n, std::vector<Key>& keys);

private:
    struct MuteSpan { int64_t m_from; int64_t m_to; };
    DVDTMF::Detector m_detector;
    int m_margin = 0;                   //!< 20 ms of samples
    std::deque<int16_t> m_delayLine;
    std::deque<MuteSpan> m_mute;        //!< input sample spans to mute, the last open while a key sounds
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODLIVEDTMF_H_
