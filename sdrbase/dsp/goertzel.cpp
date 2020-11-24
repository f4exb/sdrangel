///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "goertzel.h"

Goertzel::Goertzel(double frequency, int sampleRate) :
    m_s0(0.0),
    m_s1(0.0),
    m_s2(0.0),
    m_z(0.0),
    m_sampleCount(0)
{
    m_a = 2.0 * M_PI * frequency / sampleRate;
    m_b = 2.0 * cos(m_a);
    m_c = std::complex<double>(cos(-m_a), sin(-m_a));
}

void Goertzel::reset()
{
    m_s0 = 0.0;
    m_s1 = 0.0;
    m_s2 = 0.0;
    m_z = 0.0;
    m_sampleCount = 0;
}

void Goertzel::filter(double sample)
{
    m_s0 = m_b * m_s1 - m_s2 + sample;
    m_s2 = m_s1;
    m_s1 = m_s0;
    m_sampleCount++;
}

std::complex<double> Goertzel::goertzel(double lastSample)
{
    m_s0 = m_b * m_s1 - m_s2 + lastSample;
    m_sampleCount++;

    std::complex<double> y = m_s0 - m_s1 * m_c;
    double x = -m_a * (m_sampleCount - 1.0);
    std::complex<double> d(cos(x), sin(x));

    double scale = m_sampleCount / 2.0;
    m_z = y * d / scale;
    return m_z;
}

double Goertzel::mag()
{
    return std::abs(m_z);
}

double Goertzel::phase()
{
    return std::arg(m_z);
}
