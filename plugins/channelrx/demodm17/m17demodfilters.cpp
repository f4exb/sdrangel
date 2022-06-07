///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB.                                  //
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

#include "m17demodfilters.h"

const float M17DemodAudioInterpolatorFilter::m_lpa[3] = {1.0,           1.392667E+00, -5.474446E-01};
const float M17DemodAudioInterpolatorFilter::m_lpb[3] = {3.869430E-02,  7.738860E-02,  3.869430E-02};
// f(-3dB) = 300 Hz @ 8000 Hz SR (w = 0.075):
const float M17DemodAudioInterpolatorFilter::m_hpa[3] = {1.000000e+00,  1.667871e+00, -7.156964e-01};
const float M17DemodAudioInterpolatorFilter::m_hpb[3] = {8.459039e-01, -1.691760e+00,  8.459039e-01};

M17DemodAudioInterpolatorFilter::M17DemodAudioInterpolatorFilter() :
        m_filterLP(m_lpa, m_lpb),
        m_filterHP(m_hpa, m_hpb),
        m_useHP(false)
{
}

M17DemodAudioInterpolatorFilter::~M17DemodAudioInterpolatorFilter()
{}

float M17DemodAudioInterpolatorFilter::run(const float& sample)
{
    return m_useHP ? m_filterLP.run(m_filterHP.run(sample)) : m_filterLP.run(sample);
}

float M17DemodAudioInterpolatorFilter::runHP(const float& sample)
{
    return m_filterHP.run(sample);
}

float M17DemodAudioInterpolatorFilter::runLP(const float& sample)
{
    return m_filterLP.run(sample);
}
