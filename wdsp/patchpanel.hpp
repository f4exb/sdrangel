/*  patchpanel.h

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

#ifndef wdsp_patchpanel_h
#define wdsp_patchpanel_h

#include "export.h"

namespace WDSP {

class WDSP_API PANEL
{
public:
    int run;
    int size;
    float* in;
    float* out;
    double gain1;
    double gain2I;
    double gain2Q;
    int inselect;
    int copy;

    PANEL(
        int run,
        int size,
        float* in,
        float* out,
        double gain1,
        double gain2I,
        double gain2Q,
        int inselect,
        int copy
    );
    PANEL(const PANEL&) = delete;
    PANEL& operator=(const PANEL& other) = delete;
    ~PANEL() = default;

    void flush();
    void execute();
    void setBuffers(float* in, float* out);
    void setSamplerate(int rate);
    void setSize(int size);
    // RXA Properties
    void setRun(int run);
    void setSelect(int select);
    void setGain1(double gain);
    void setGain2(double gainI, double gainQ);
    void setPan(double pan);
    void setCopy(int copy);
    void setBinaural(int bin);
    // TXA Properties
    void setSelectTx(int select);
};

} // namespace WDSP

#endif
