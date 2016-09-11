///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "filtermbe.h"

const float MBEAudioInterpolatorFilter::m_a0 = 3.869430E-02;
const float MBEAudioInterpolatorFilter::m_a1 = 7.738860E-02;
const float MBEAudioInterpolatorFilter::m_a2 = 3.869430E-02;
const float MBEAudioInterpolatorFilter::m_b1 = 1.392667E+00;
const float MBEAudioInterpolatorFilter::m_b2 = -5.474446E-01;

MBEAudioInterpolatorFilter::MBEAudioInterpolatorFilter()
{
    init();
}

MBEAudioInterpolatorFilter::~MBEAudioInterpolatorFilter()
{}

void MBEAudioInterpolatorFilter::init()
{
    m_x[0] = 0.0f;
    m_x[1] = 0.0f;
    m_y[0] = 0.0f;
    m_y[1] = 0.0f;
}

float MBEAudioInterpolatorFilter::run(float sample)
{
    float y = m_a0*sample + m_a1*m_x[0] + m_a2*m_x[1] + m_b1*m_y[0] + m_b2*m_y[1]; // this is y[n]

    m_x[1] = m_x[0];
    m_x[0] = sample;

    m_y[1] = m_y[0];
    m_y[0] = y;

    return y;
}



