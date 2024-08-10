/*  fcurve.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016 Warren Pratt, NR0V
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

#ifndef wdsp_fcurve_h
#define wdsp_fcurve_h

#include <vector>

#include "export.h"

namespace WDSP {

class WDSP_API FCurve
{
public:
    static void fc_impulse (std::vector<float>& impulse, int nc, float f0, float f1, float g0, float g1, int curve, float samplerate, float scale, int ctfmode, int wintype);
    static void fc_mults   (std::vector<float>& mults, int size, float f0, float f1, float g0, float g1, int curve, float samplerate, float scale, int ctfmode, int wintype);
};

} // namespace WDSP

#endif
