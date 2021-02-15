///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#ifndef SDRBASE_DSP_GLSPECTRUMINTERFACE_H_
#define SDRBASE_DSP_GLSPECTRUMINTERFACE_H_

#include <vector>
#include "dsptypes.h"

class GLSpectrumInterface
{
public:
    GLSpectrumInterface() {}
    virtual ~GLSpectrumInterface() {}
    virtual void newSpectrum(const Real* spectrum, int nbBins, int fftSize)
    {
        (void) spectrum;
        (void) nbBins;
        (void) fftSize;
    }
};

#endif // SDRBASE_DSP_GLSPECTRUMINTERFACE_H_
