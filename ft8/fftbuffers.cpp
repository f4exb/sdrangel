///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
// reformatted and adapted to Qt and SDRangel context                            //
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
#include "fftbuffers.h"

namespace FT8
{

FFTBuffers::~FFTBuffers()
{
    for (auto& mapitem : m_rs) {
        fftwf_free(mapitem.second);
    }
    for (auto& mapitem : m_cs) {
        fftwf_free(mapitem.second);
    }
    for (auto& mapitem : m_ccis) {
        fftwf_free(mapitem.second);
    }
    for (auto& mapitem : m_ccos) {
        fftwf_free(mapitem.second);
    }
}

float* FFTBuffers::getR(int n)
{
    if (m_rs.find(n) == m_rs.end()) {
        m_rs[n] = (float *) fftwf_malloc(sizeof(float) * n);
    }

    return  m_rs[n];
}

fftwf_complex *FFTBuffers::getC(int n)
{
    if (m_cs.find(n) == m_cs.end()) {
        m_cs[n] = (fftwf_complex *) fftwf_malloc(sizeof(fftwf_complex) * ((n / 2) + 1));
    }

    return m_cs[n];
}

fftwf_complex *FFTBuffers::getCCI(int n)
{
    if (m_ccis.find(n) == m_ccis.end()) {
        m_ccis[n] = (fftwf_complex *) fftwf_malloc(n * sizeof(fftwf_complex));
    }

    return m_ccis[n];
}

fftwf_complex *FFTBuffers::getCCO(int n)
{
    if (m_ccos.find(n) == m_ccos.end()) {
        m_ccos[n] = (fftwf_complex *) fftwf_malloc(n * sizeof(fftwf_complex));
    }

    return m_ccos[n];
}

} // nemespace FT8
