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

#include <cstdint>
#include <limits>

#include "dsemodlivedtmf.h"

void DSEModLiveDTMF::setSampleRate(int sampleRate)
{
    m_detector.setSampleRate(sampleRate);
    m_margin = sampleRate / 50;
    reset();
}

void DSEModLiveDTMF::reset()
{
    m_detector.reset();
    m_delayLine.clear();
    m_mute.clear();
}

void DSEModLiveDTMF::process(int16_t *pcm, int n, std::vector<Key>& keys)
{
    const int rate = m_detector.getSampleRate();
    const int delay = getDelay();
    const int64_t open = std::numeric_limits<int64_t>::max();

    if (m_delayLine.empty()) {
        m_delayLine.assign(delay, 0);
    }

    const int64_t first = m_detector.samples();     // the input index of pcm[0]
    std::vector<float> x(n);

    for (int i = 0; i < n; i++) {
        x[i] = pcm[i] / 32768.0f;
    }

    std::vector<DVDTMF::Detector::Key> found;
    m_detector.process(x.data(), n, found);

    for (const DVDTMF::Detector::Key& k : found)
    {
        const int64_t to = k.m_start + (int64_t) k.m_lengthMs * rate / 1000 + m_margin;

        if (!m_mute.empty() && (m_mute.back().m_to == open)) {
            m_mute.back().m_to = to;
        } else {
            m_mute.push_back(MuteSpan{ k.m_start - m_margin, to });
        }

        Key key;
        key.m_key = k.m_key;
        key.m_lengthMs = k.m_lengthMs;
        key.m_newSequence = (k.m_gapMs < 0) || (k.m_gapMs >= 2000);
        key.m_gapMs = k.m_gapMs < 0 ? 100 : k.m_gapMs;
        keys.push_back(key);
    }

    // A key sounding: muted from its start on, until it ends
    if (m_detector.sounding() && (m_mute.empty() || (m_mute.back().m_to != open))) {
        m_mute.push_back(MuteSpan{ m_detector.soundingSince() - m_margin, open });
    }

    for (int i = 0; i < n; i++)
    {
        m_delayLine.push_back(pcm[i]);
        const int16_t out = m_delayLine.front();
        m_delayLine.pop_front();
        const int64_t k = first + i - delay;    // the input index of the sample going out

        while (!m_mute.empty() && (m_mute.front().m_to < k)) {
            m_mute.pop_front();
        }

        pcm[i] = (!m_mute.empty() && (k >= m_mute.front().m_from)) ? 0 : out;
    }
}
