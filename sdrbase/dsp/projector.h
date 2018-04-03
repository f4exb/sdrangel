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

#include "dsptypes.h"

class SymbolSynchronizer;

class Projector
{
public:
    enum ProjectionType
    {
        ProjectionReal = 0, //!< Extract real part
        ProjectionImag,     //!< Extract imaginary part
        ProjectionMagLin,   //!< Calculate linear magnitude or modulus
        ProjectionMagDB,    //!< Calculate logarithmic (dB) of squared magnitude
        ProjectionPhase,    //!< Calculate phase
        ProjectionDPhase,   //!< Calculate phase derivative i.e. instantaneous frequency scaled to sample rate
        ProjectionClock,    //!< Clock projection (symbol synchronization)
		nbProjectionTypes   //!< Gives the number of projections in the enum
    };

    Projector(ProjectionType projectionType);
    ~Projector();

    ProjectionType getProjectionType() const { return m_projectionType; }
    void settProjectionType(ProjectionType projectionType) { m_projectionType = projectionType; }
    void setCache(Real *cache) { m_cache = cache; }
    void setCacheMaster(bool cacheMaster) { m_cacheMaster = cacheMaster; }

    Real run(const Sample& s);

private:
    ProjectionType m_projectionType;
    Real m_prevArg;
    Real *m_cache;
    bool m_cacheMaster;
    SymbolSynchronizer *m_symSync;
};
