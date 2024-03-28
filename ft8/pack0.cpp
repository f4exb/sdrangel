///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "pack0.h"

namespace FT8 {

void pa128(int a77[], int start, int len, const boost::multiprecision::int128_t value)
{
    boost::multiprecision::int128_t x = value;
    int i = start + len;

    while (x != 0)
    {
        i--;
        a77[i] = (int) (x % 2);
        x /= 2;
    }
}

void pa64(int a77[], int start, int len, const uint64_t value)
{
    uint64_t x = value;
    int i = start + len;

    while (x != 0)
    {
        i--;
        a77[i] = x % 2;
        x /= 2;
    }
}

} // namespae FT8
