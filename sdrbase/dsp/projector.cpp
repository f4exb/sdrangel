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

#include <math.h>
#include "projector.h"

Projector::Projector(ProjectionType projectionType) :
    m_projectionType(projectionType),
    m_prevArg(0.0f),
    m_cache(0),
    m_cacheMaster(true)
{
}

Projector::~Projector()
{
}

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
        case ProjectionMagSq:
        {
            Real re = s.m_real / SDR_RX_SCALEF;
            Real im = s.m_imag / SDR_RX_SCALEF;
            v = re*re + im*im;
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
        case ProjectionBPSK:
        {
            Real arg = std::atan2((float) s.m_imag, (float) s.m_real);
            v = normalizeAngle(2*arg) / (2.0*M_PI); // generic estimation around 0
            // mapping on 2 symbols
            if (arg < -M_PI/2) {
                v -= 1.0/2;
            } else if (arg < M_PI/2) {
                v += 1.0/2;
            } else if (arg < M_PI) {
                v -= 1.0/2;
            }
        }
            break;
        case ProjectionQPSK:
        {
            Real arg = std::atan2((float) s.m_imag, (float) s.m_real);
            v = normalizeAngle(4*arg) / (4.0*M_PI); // generic estimation around 0
            // mapping on 4 symbols
            if (arg < -3*M_PI/4) {
                v -= 3.0/4;
            } else if (arg < -M_PI/4) {
                v -= 1.0/4;
            } else if (arg < M_PI/4) {
                v += 1.0/4;
            } else if (arg < 3*M_PI/4) {
                v += 3.0/4;
            } else if (arg < M_PI) {
                v -= 3.0/4;
            }
        }
            break;
        case Projection8PSK:
        {
            Real arg = std::atan2((float) s.m_imag, (float) s.m_real);
            v = normalizeAngle(8*arg) / (8.0*M_PI); // generic estimation around 0
            // mapping on 8 symbols
            if (arg < -7*M_PI/8) {
               v -= 7.0/8;
            } else if (arg < -5*M_PI/8) {
                v -= 5.0/8;
            } else if (arg < -3*M_PI/8) {
                v -= 3.0/8;
            } else if (arg < -M_PI/8) {
                v -= 1.0/8;
            } else if (arg < M_PI/8) {
                v += 1.0/8;
            } else if (arg < 3*M_PI/8) {
                v += 3.0/8;
            } else if (arg < 5*M_PI/8) {
                v += 5.0/8;
            } else if (arg < 7*M_PI/8) {
                v += 7.0/8;
            } else if (arg < M_PI) {
                v -= 7.0/8;
            }
        }
            break;
        case Projection16PSK:
        {
            Real arg = std::atan2((float) s.m_imag, (float) s.m_real);
            v = normalizeAngle(16*arg) / (16.0*M_PI); // generic estimation around 0
            // mapping on 16 symbols
            if (arg < -15*M_PI/16) {
               v -= 15.0/16;
            } else if (arg < -13*M_PI/16) {
                v -= 13.0/6;
            } else if (arg < -11*M_PI/16) {
                v -= 11.0/16;
            } else if (arg < -9*M_PI/16) {
                v -= 9.0/16;
            } else if (arg < -7*M_PI/16) {
                v -= 7.0/16;
            } else if (arg < -5*M_PI/16) {
                v -= 5.0/16;
            } else if (arg < -3*M_PI/16) {
                v -= 3.0/16;
            } else if (arg < -M_PI/16) {
                v -= 1.0/16;
            } else if (arg < M_PI/16) {
                v += 1.0/16;
            } else if (arg < 3.0*M_PI/16) {
                v += 3.0/16;
            } else if (arg < 5.0*M_PI/16) {
                v += 5.0/16;
            } else if (arg < 7.0*M_PI/16) {
                v += 7.0/16;
            } else if (arg < 9.0*M_PI/16) {
                v += 9.0/16;
            } else if (arg < 11.0*M_PI/16) {
                v += 11.0/16;
            } else if (arg < 13.0*M_PI/16) {
                v += 13.0/16;
            } else if (arg < 15.0*M_PI/16) {
                v += 15.0/16;
            } else if (arg < M_PI) {
                v -= 15.0/16;
            }
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

Real Projector::normalizeAngle(Real angle)
{
    while (angle <= -M_PI) {
        angle += 2.0*M_PI;
    }
    while (angle > M_PI) {
        angle -= 2.0*M_PI;
    }
    return angle;
}


