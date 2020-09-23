///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_POPCOUNT_H
#define INCLUDE_POPCOUNT_H

// Population count - count number of bits
#if defined(__cplusplus) && (__cplusplus >= 202002L)
#include <bit>
#define popcount std::popcount
#elif defined (__GNUC__)
#define popcount __builtin_popcount
#elif defined(_MSC_VER)
#include <intrin.h>
#define popcount __popcnt
#else
static int popcount(int in)
{
    int cnt = 0;
    for(int i = 0; i < 32; i++)
        cnt += (in >> i) & 1;
    return cnt;
}
#endif

#endif /* INCLUDE_POPCOUNT_H */
