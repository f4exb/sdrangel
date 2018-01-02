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

const float MBEAudioInterpolatorFilter::m_lpa[3] = {1.0,           1.392667E+00, -5.474446E-01};
const float MBEAudioInterpolatorFilter::m_lpb[3] = {3.869430E-02,  7.738860E-02,  3.869430E-02};

const float MBEAudioInterpolatorFilter::m_hpa[3] = {1.000000e+00,  1.955578e+00, -9.565437e-01};
const float MBEAudioInterpolatorFilter::m_hpb[3] = {9.780305e-01, -1.956061e+00,  9.780305e-01};

MBEAudioInterpolatorFilter::MBEAudioInterpolatorFilter() :
        m_filterLP(m_lpa, m_lpb),
        m_filterHP(m_hpa, m_hpb),
        m_useHP(false)
{
}

MBEAudioInterpolatorFilter::~MBEAudioInterpolatorFilter()
{}

float MBEAudioInterpolatorFilter::run(const float& sample)
{
    return m_useHP ? m_filterLP.run(m_filterHP.run(sample)) : m_filterLP.run(sample);
}
