// Copyright (C) 2026 SDRangel contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Peak/valley tracking and transition-directed clock recovery follow the approach
// in Dire Wolf's demod_9600.c, Copyright (C) 2011, 2012, 2013, 2015, 2019,
// 2021 John Langner, WB2OSZ (GPL-2.0-or-later). This implementation uses a bounded
// floating-point phase and interpolates both crossings and symbol samples.

#ifndef INCLUDE_PACKETDEMODG3RUH_H
#define INCLUDE_PACKETDEMODG3RUH_H

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "dsp/firfilter.h"

// FM discriminator samples in; descrambled NRZI levels out. HDLC/NRZI decoding
// remains in PacketDemodFramer. No allocations in process(), no per-frame reset:
// the G3RUH scrambler also runs during flags and synchronizes after 17 symbols.
class PacketDemodG3RUH
{
public:
    static constexpr int SampleRate = 38400;
    static constexpr int BaudRate = 9600;

    PacketDemodG3RUH() { reset(); }

    void reset()
    {
        m_lowpass.create(31, SampleRate, 6500.0f);
        m_peak = 0.0f;
        m_valley = 0.0f;
        m_previous = 0.0f;
        m_phase = 0.0f;
        m_history = 0;
    }

    bool process(float sample, int& symbol)
    {
        const float filtered = m_lowpass.filter(sample);
        // The midpoint tracks discriminator DC (residual carrier offset); the
        // span normalizes deviation, so no separate FM deviation setting is needed.
        // Decay over about 26 ms: RF-filter startup transients must not bias the
        // slicer for the remainder of a short packet preamble.
        m_peak += (filtered > m_peak ? 0.08f : 0.001f) * (filtered - m_peak);
        m_valley += (filtered < m_valley ? 0.08f : 0.001f) * (filtered - m_valley);
        const float span = std::max(m_peak - m_valley, 1.0e-6f);
        const float value = (2.0f * filtered - m_peak - m_valley) / span;

        constexpr float step = static_cast<float>(BaudRate) / SampleRate;
        m_phase += step;
        const bool ready = m_phase >= 0.5f;

        if (ready)
        {
            // Sample the eye center, including when it lies between input samples.
            const float fraction = (m_phase - 0.5f) / step;
            const float decision = value + fraction * (m_previous - value);
            const uint32_t received = decision >= 0.0f ? 1u : 0u;
            symbol = (received ^ (m_history >> 11) ^ (m_history >> 16)) & 1u;
            // Feed back RECEIVED scrambled levels, not descrambler output.
            m_history = ((m_history << 1) | received) & 0x1ffffu;
            m_phase -= 1.0f;
        }

        if ((value > 0.0f && m_previous < 0.0f)
            || (value < 0.0f && m_previous > 0.0f))
        {
            // Phase zero is a symbol boundary. Interpolate the crossing so the
            // four-sample clock does not simply lock to the next input sample.
            const float target = step * value / (value - m_previous);
            m_phase += 0.2f * (target - m_phase);
        }

        m_previous = value;
        return ready;
    }

private:
    Lowpass<Real> m_lowpass;
    float m_peak;
    float m_valley;
    float m_previous;
    float m_phase;
    uint32_t m_history;
};

#endif // INCLUDE_PACKETDEMODG3RUH_H
