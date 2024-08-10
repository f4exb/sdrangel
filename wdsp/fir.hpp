/*  fir.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2022 Warren Pratt, NR0V
Copyright (C) 2024 Edouard Griffiths, F4EXB Adapted to SDRangel

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@pratt.one

*/
#ifndef wdsp_fir_h
#define wdsp_fir_h

#include <vector>

#include "export.h"

namespace WDSP {

class WDSP_API FIR
{
public:
    static void fftcv_mults (std::vector<float>& mults, int NM, const float* impulse);
    static void fir_fsamp_odd (std::vector<float>& c_impulse, int N, const float* A, int rtype, double scale, int wintype);
    static void fir_fsamp (std::vector<float>& c_impulse, int N, const float* A, int rtype, double scale, int wintype);
    static void fir_bandpass (std::vector<float>& impulse, int N, double f_low, double f_high, double samplerate, int wintype, int rtype, double scale);
    static void mp_imp (int N, std::vector<float>& fir, std::vector<float>& mpfir, int pfactor, int polarity);

private:
    static void analytic (int N, float* in, float* out);
    static void get_fsamp_window(std::vector<float>& window, int N, int wintype);
    static void fir_read (std::vector<float>& impulse, int N, const char *filename, int rtype, float scale);
    static void zff_impulse(std::vector<float>& impulse, int nc, float scale);
};

#endif

} // namespace WDSP
