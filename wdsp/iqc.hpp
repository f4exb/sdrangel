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

#include <array>
#include <vector>

#include "export.h"

namespace WDSP {

class TXA;

class WDSP_API IQC
{
public:
    enum class IQCSTATE
    {
        RUN = 0,
        BEGIN,
        SWAP,
        END,
        DONE
    };

    long run;
    long busy;
    int size;
    float* in;
    float* out;
    double rate;
    int ints;
    std::vector<double> t;
    int cset;
    std::array<std::vector<double>, 2> cm;
    std::array<std::vector<double>, 2> cc;
    std::array<std::vector<double>, 2> cs;
    double tup;
    std::vector<double> cup;
    int count;
    int ntup;
    IQCSTATE state;
    struct
    {
        int spi;
        std::vector<int> cpi;
        int full_ints;
        int count;
    } dog;

    IQC(
        int run,
        int size,
        float* in,
        float* out,
        double rate,
        int ints,
        double tup,
        int spi
    );
    IQC(const IQC&) = delete;
    IQC& operator=(const IQC& other) = delete;
    ~IQC() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);

private:
    void size_iqc();
    void calc();
};

} // namespace WDSP

#endif
