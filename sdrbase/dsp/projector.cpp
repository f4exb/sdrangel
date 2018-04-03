///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#include "projector.h"
#include "symsync.h" // dependency on liquid-dsp

Projector::Projector(ProjectionType projectionType) :
    m_projectionType(projectionType),
    m_prevArg(0.0f),
    m_cache(0),
    m_cacheMaster(true)
{}

Projector::~Projector()
{}

Real Projector::run(const Sample& s)
{
    Real v;

    if ((m_cache) && !m_cacheMaster) {
        return m_cache[(int) m_projectionType];
    }
    else
    {
        switch (m_projectionType)
        {
        case ProjectionImag:
            v = s.m_imag / SDR_RX_SCALEF;
            break;
        case ProjectionMagLin:
        {
            Real re = s.m_real / SDR_RX_SCALEF;
            Real im = s.m_imag / SDR_RX_SCALEF;
            Real magsq = re*re + im*im;
            v = std::sqrt(magsq);
        }
            break;
        case ProjectionMagDB:
        {
            Real re = s.m_real / SDR_RX_SCALEF;
            Real im = s.m_imag / SDR_RX_SCALEF;
            Real magsq = re*re + im*im;
            v = log10f(magsq) * 10.0f;
        }
            break;
        case ProjectionPhase:
            v = std::atan2((float) s.m_imag, (float) s.m_real) / M_PI;
            break;
        case ProjectionDPhase:
        {
            Real curArg = std::atan2((float) s.m_imag, (float) s.m_real);
            Real dPhi = (curArg - m_prevArg) / M_PI;
            m_prevArg = curArg;

            if (dPhi < -1.0f) {
                dPhi += 2.0f;
            } else if (dPhi > 1.0f) {
                dPhi -= 2.0f;
            }

            v = dPhi;
        }
            break;
        case ProjectionReal:
        default:
            v = s.m_real / SDR_RX_SCALEF;
            break;
        }

        if (m_cache) {
            m_cache[(int) m_projectionType] = v;
        }

        return v;
    }
}

