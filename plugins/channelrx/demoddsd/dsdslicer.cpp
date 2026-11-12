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

#include <algorithm>
#include <cmath>

#include "dsdslicer.h"

DSDSlicer::DSDSlicer() :
    m_fourLevel(true),
    m_outer(1944.0f),
    m_offset(0.0f),
    m_sigma2(1.0e4f),
    m_trackRate(0.02f)
{
}

void DSDSlicer::initialise(float outerLevelHz, float offsetHz, float sigma2)
{
    m_outer = std::max(50.0f, outerLevelHz);
    m_offset = offsetHz;
    m_sigma2 = std::max(1.0f, sigma2);
}

float DSDSlicer::getSnrDB() const
{
    // Mean symbol power over the four levels relative to the noise variance
    float inner = m_outer / 3.0f;
    float meanPower = m_fourLevel ? 0.5f * (m_outer * m_outer + inner * inner) : m_outer * m_outer;
    return 10.0f * std::log10(std::max(1e-6f, meanPower / m_sigma2));
}

DSDSlicer::Symbol DSDSlicer::slice(float y, float weight, bool track)
{
    Symbol s;
    float step = m_outer / 3.0f;
    float u = (y - m_offset) / step;
    s.m_u = u;

    if (m_fourLevel)
    {
        s.m_level = u > 2.0f ? 3 : u > 0.0f ? 1 : u > -2.0f ? -1 : -3;
    }
    else
    {
        s.m_level = u > 0.0f ? 3 : -3;
    }

    static const uint8_t dibits[7] = { 3, 0, 2, 0, 0, 0, 1 }; // index level + 3
    s.m_dibit = dibits[s.m_level + 3];

    // Max-log LLRs in normalised units: noise variance of u is sigma2 / step^2
    float sigmaU2 = std::max(1e-4f, m_sigma2 / (step * step));
    float scale = weight / (2.0f * sigmaU2);
    auto d2 = [u](float level) { float d = u - level; return d * d; };

    if (m_fourLevel)
    {
        float dPos = std::min(d2(3.0f), d2(1.0f));
        float dNeg = std::min(d2(-1.0f), d2(-3.0f));
        float dOuter = std::min(d2(3.0f), d2(-3.0f));
        float dInner = std::min(d2(1.0f), d2(-1.0f));
        s.m_llrMsb = (dNeg - dPos) * scale;   // bit 1 = 1 for negative levels
        s.m_llrLsb = (dOuter - dInner) * scale; // bit 0 = 1 for outer levels
        float nearestBoundary = std::min(std::fabs(u), std::min(std::fabs(u - 2.0f), std::fabs(u + 2.0f)));
        s.m_reliability = std::min(1.0f, nearestBoundary);
    }
    else
    {
        s.m_llrMsb = (d2(-3.0f) - d2(3.0f)) * scale;
        s.m_llrLsb = 0.0f;
        s.m_reliability = std::min(1.0f, std::fabs(u) / 3.0f);
    }

    if (track && (s.m_reliability > 0.4f) && (weight > 0.3f))
    {
        // Decision directed tracking of the level, the offset and the noise
        float level = (float) s.m_level;
        float estimatedStep = (y - m_offset) / level;           // step from this symbol
        float estimatedOffset = y - level * step;
        float residual = y - m_offset - level * step;
        float rate = m_trackRate;
        float newOuter = m_outer + rate * (3.0f * estimatedStep - m_outer);
        m_outer = std::max(0.9f * m_outer, std::min(1.1f * m_outer, newOuter));
        m_offset += rate * (estimatedOffset - m_offset);
        m_sigma2 += rate * (residual * residual - m_sigma2);
        m_sigma2 = std::max(1.0f, m_sigma2);
    }

    return s;
}
