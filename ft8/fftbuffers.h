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

#ifndef FFTBUFFERS_H
#define FFTBUFFERS_H

#include <map>
#include <fftw3.h>

#include "export.h"

namespace FT8
{

class FT8_API FFTBuffers
{
public:
    ~FFTBuffers();

    float* getR(int n);
    fftwf_complex *getC(int n);
    fftwf_complex *getCCI(int n);
    fftwf_complex *getCCO(int n);

private:
    std::map<int, float*> m_rs; //!< R2C inputs or C2R inputs by size
    std::map<int, fftwf_complex*> m_cs; //!< R2C outputs or C2R inputs by size
    std::map<int, fftwf_complex*> m_ccis; //!< C2C inputs/outputs by size
    std::map<int, fftwf_complex*> m_ccos; //!< C2C outputs/inputs by size
};

} // FFTBuffers

#endif // FFTBUFFERS_H
