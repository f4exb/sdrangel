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

#include "export.h"

namespace WDSP {

class WDSP_API FIR
{
public:
    static double* fftcv_mults (int NM, double* c_impulse);
    static double* fir_fsamp_odd (int N, double* A, int rtype, double scale, int wintype);
    static double* fir_fsamp (int N, double* A, int rtype, double scale, int wintype);
    static double* fir_bandpass (int N, double f_low, double f_high, double samplerate, int wintype, int rtype, double scale);
    static double* get_fsamp_window(int N, int wintype);
    static double *fir_read (int N, const char *filename, int rtype, double scale);
    static void mp_imp (int N, double* fir, double* mpfir, int pfactor, int polarity);
    static double* zff_impulse(int nc, double scale);

private:
    static void analytic (int N, double* in, double* out);
};

#endif

} // namespace WDSP
