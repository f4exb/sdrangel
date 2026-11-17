///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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
#include <cmath>

#include "radiopagingvoiceprocessor.h"

namespace {

const int filterTaps = 301;
const float agcTarget = 0.8f;           // Peak level of the voice into the compressor (-2 dBFS)
const float agcMaxGain = 31.6f;         // +30 dB at most
const float agcMinGain = 0.1f;          // -20 dB
const float gateLevel = 0.002f;         // -54 dBFS: below it the voice is a pause, the gain is held
const float compressorPredelay = 0.006f;

float coefficient(float seconds, int sampleRate)
{
    return 1.0f - std::exp(-1.0f / (seconds * (float) sampleRate));
}

} // namespace

constexpr float RadioPagingVoiceProcessor::m_lowCutoff;
constexpr float RadioPagingVoiceProcessor::m_highCutoff;
constexpr float RadioPagingVoiceProcessor::m_peak;

RadioPagingVoiceProcessor::RadioPagingVoiceProcessor(int sampleRate) :
    m_sampleRate(sampleRate),
    m_delay(filterTaps - 1 + (int) (compressorPredelay * sampleRate))
{
    reset();
}

void RadioPagingVoiceProcessor::reset()
{
    m_bandpass.create(filterTaps, m_sampleRate, m_lowCutoff, m_highCutoff);
    m_lowpass.create(filterTaps, m_sampleRate, m_highCutoff);
    m_compressor.initSimple(
        m_sampleRate,
        0.0f,       // pregain (dB): the gain is set before
        -18.0f,     // threshold (dB)
        12.0f,      // knee (dB)
        4.0f,       // ratio: the level still varies
        0.003f,     // attack (s)
        0.25f       // release (s)
    );
    m_compressor.m_postgain = 3.0f;    // dB: a settled voice peaks near the full scale
    m_compressor.initState();
    m_envelope = 0.0f;
    m_gain = 1.0f;
    m_attack = coefficient(0.005f, m_sampleRate);
    m_release = coefficient(1.0f, m_sampleRate);
    m_gainRate = coefficient(0.5f, m_sampleRate);
}

float RadioPagingVoiceProcessor::process(float sample)
{
    float x = m_bandpass.filter(sample);
    float magnitude = std::fabs(x);

    // Peak envelope: fast up, slow down, so that the syllables keep their levels
    m_envelope += ((magnitude > m_envelope) ? m_attack : m_release) * (magnitude - m_envelope);

    // The gain goes down at once on a louder voice, up slowly, and is held in pauses
    if (m_envelope > gateLevel)
    {
        float target = std::max(agcMinGain, std::min(agcMaxGain, agcTarget / m_envelope));

        if (target < m_gain) {
            m_gain += m_attack * (target - m_gain);
        } else {
            m_gain += m_gainRate * (target - m_gain);
        }
    }

    float y = m_compressor.compress(x * m_gain);
    y = std::max(-m_peak, std::min(m_peak, y));
    y = m_lowpass.filter(y);

    return std::max(-1.0f, std::min(1.0f, y));
}
