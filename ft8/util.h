///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
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
#ifndef UTIL_H
#define UTIL_H

#include <vector>
#include <complex>

namespace FT8 {

double now();
std::complex<float> goertzel(std::vector<float> v, int rate, int i0, int n, float hz);
float vmax(const std::vector<float> &v);
std::vector<float> vreal(const std::vector<std::complex<float>> &a);
std::vector<float> vimag(const std::vector<std::complex<float>> &a);
std::vector<std::complex<float>> gfsk_c(
    const std::vector<int> &symbols,
    float hz0, float hz1,
    float spacing, int rate, int symsamples,
    float phase0,
    const std::vector<float> &gwin
);
std::vector<float> gfsk_r(
    const std::vector<int> &symbols,
    float hz0, float hz1,
    float spacing, int rate, int symsamples,
    float phase0,
    const std::vector<float> &gwin
);
std::vector<float> gfsk_window(int samples_per_symbol, float b);
std::string trim(const std::string &s);

typedef unsigned long ulong;
typedef unsigned int uint;

} // namespace FT8

#endif
