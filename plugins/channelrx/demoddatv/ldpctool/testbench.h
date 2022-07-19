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
