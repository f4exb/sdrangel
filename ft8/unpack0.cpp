///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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
#include "unpack0.h"
#include "unpack0.h"

namespace FT8 {

//
// turn bits into a 128-bit integer.
// most significant bit first.
//

boost::multiprecision::int128_t un128(int a77[], int start, int len)
{
    boost::multiprecision::int128_t x = 0;

    // assert(len < (int)sizeof(x) * 8 && start >= 0 && start + len <= 77);
    for (int i = 0; i < len; i++)
    {
        x <<= 1;
        x |= a77[start + i];
    }

    return x;
}

//
// turn bits into a 64-bit integer.
// most significant bit first.
//
uint64_t un64(int a77[], int start, int len)
{
    uint64_t x = 0;

    // assert(len < (int)sizeof(x) * 8 && start >= 0 && start + len <= 63);
    for (int i = 0; i < len; i++)
    {
        x <<= 1;
        x |= a77[start + i];
    }

    return x;
}

} // namespace FT8
