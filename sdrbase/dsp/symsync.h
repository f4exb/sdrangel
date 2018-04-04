///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Symbol synchronizer or symbol clock recovery mostly encapsulating             //
// liquid-dsp's symsync "object"                                                 //
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

#include "dsp/dsptypes.h"
#include "liquid.h"
#include <complex.h>

class SymbolSynchronizer
{
public:
    SymbolSynchronizer();
    ~SymbolSynchronizer();

    Real run(const Sample& s);
    liquid_float_complex runZ(const Sample& s);

private:
    symsync_crcf m_sync;
    liquid_float_complex m_z[4+4]; // 4 samples per symbol. One symbol plus extra space
    liquid_float_complex m_z0;
    int m_syncSampleCount;
};
