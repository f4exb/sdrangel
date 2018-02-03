///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
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

#ifndef SDRBASE_UTIL_FIXEDTRAITS_H_
#define SDRBASE_UTIL_FIXEDTRAITS_H_

#include <stdint.h>

template<uint32_t IntBits>
class FixedTraits
{
};

template<>
struct FixedTraits<28>
{
    static const uint32_t fixed_resolution_shift = 28; //!< 1.0 representation. 28 is the highest power of two that can represent 9.99999... safely on 64 bits internally
    static const int64_t  fixed_resolution       = 1LL << fixed_resolution_shift;
    static const int32_t  max_power              = 63 - fixed_resolution_shift;
    static const int64_t  internal_pi            = 0x3243f6a8;
    static const int64_t  internal_two_pi        = 0x6487ed51;
    static const int64_t  internal_half_pi       = 0x1921fb54;
    static const int64_t  internal_quarter_pi    = 0xc90fdaa;
    static const int64_t  log_two_power_n_reversed[35]; // 35 = 63 - 28
    static const int64_t  log_one_plus_two_power_minus_n[28];
    static const int64_t  log_one_over_one_minus_two_power_minus_n[28];
    static const int64_t  arctantab[32];
};

#endif /* SDRBASE_UTIL_FIXEDTRAITS_H_ */
