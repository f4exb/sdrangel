///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include "recursivefilters.h"

#undef M_PI
#define M_PI		3.14159265358979323846

SecondOrderRecursiveFilter::SecondOrderRecursiveFilter(float samplingFrequency, float centerFrequency, float r) :
        m_r(r),
        m_frequencyRatio(centerFrequency/samplingFrequency)
{
    init();
}

SecondOrderRecursiveFilter::~SecondOrderRecursiveFilter()
{}

void SecondOrderRecursiveFilter::setFrequencies(float samplingFrequency, float centerFrequency)
{
    m_frequencyRatio = centerFrequency / samplingFrequency;
    init();
}

void SecondOrderRecursiveFilter::setR(float r)
{
    m_r = r;
    init();
}

short SecondOrderRecursiveFilter::run(short sample)
{
    m_v[0] = ((1.0f - m_r) * (float) sample) + (2.0f * m_r * cos(2.0*M_PI*m_frequencyRatio) * m_v[1]) - (m_r * m_r * m_v[2]);
    float y = m_v[0] - m_v[2];
    m_v[2] = m_v[1];
    m_v[1] = m_v[0];

    return (short) y;
}

float SecondOrderRecursiveFilter::run(float sample)
{
    m_v[0] = ((1.0f - m_r) * sample) + (2.0f * m_r * cos(2.0*M_PI*m_frequencyRatio) * m_v[1]) - (m_r * m_r * m_v[2]);
    float y = m_v[0] - m_v[2];
    m_v[2] = m_v[1];
    m_v[1] = m_v[0];

    return y;
}

void SecondOrderRecursiveFilter::init()
{
    for (int i = 0; i < 3; i++)
    {
        m_v[i] = 0.0f;
    }
}

