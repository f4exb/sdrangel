/*  iqc.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
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

warren@wpratt.com

*/

#ifndef wdsp_iqc_h
#define wdsp_iqc_h

#include <atomic>
#include <QRecursiveMutex>

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API IQC
{
public:
    std::atomic<long> run;
    std::atomic<long> busy;
    int size;
    double* in;
    double* out;
    double rate;
    int ints;
    double* t;
    int cset;
    double* cm[2];
    double* cc[2];
    double* cs[2];
    double tup;
    double* cup;
    int count;
    int ntup;
    int state;
    struct
    {
        int spi;
        int* cpi;
        int full_ints;
        int count;
        QRecursiveMutex cs;
    } dog;

    static IQC* create_iqc (int run, int size, double* in, double* out, double rate, int ints, double tup, int spi);
    static void destroy_iqc (IQC *a);
    static void flush_iqc (IQC *a);
    static void xiqc (IQC *a);
    static void setBuffers_iqc (IQC *a, double* in, double* out);
    static void setSamplerate_iqc (IQC *a, int rate);
    static void setSize_iqc (IQC *a, int size);
    // TXA Properties
    static void GetiqcValues (TXA& txa, double* cm, double* cc, double* cs);
    static void SetiqcValues (TXA& txa, double* cm, double* cc, double* cs);
    static void SetiqcSwap (TXA& txa, double* cm, double* cc, double* cs);
    static void SetiqcStart (TXA& txa, double* cm, double* cc, double* cs);
    static void SetiqcEnd (TXA& txa);
    static void GetiqcDogCount (TXA& txa, int* count);
    static void SetiqcDogCount (TXA& txa, int  count);

private:
    static void size_iqc (IQC *a);
    static void desize_iqc (IQC *a);
    static void calc_iqc (IQC *a);
    static void decalc_iqc (IQC *a);
};

} // namespace WDSP

#endif
