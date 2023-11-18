///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
/*
LDPC testbench

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#pragma once

#include <cstdint>
#include <complex>
#include "simd.h"

namespace ldpctool {

#ifdef __AVX2__
const int SIZEOF_SIMD = 32;
#else
const int SIZEOF_SIMD = 16;
#endif

typedef float value_type;
typedef std::complex<value_type> complex_type;

#if 1
typedef int8_t code_type;
const int FACTOR = 2;
#else
typedef float code_type;
const int FACTOR = 1;
#endif

#if 0
const int SIMD_WIDTH = 1;
typedef code_type simd_type;
#else
const int SIMD_WIDTH = SIZEOF_SIMD / sizeof(code_type);
typedef SIMD<code_type, SIMD_WIDTH> simd_type;
#endif

} // namepsace ldpctool
