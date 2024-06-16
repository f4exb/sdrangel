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

class RXA;
class TXA;

class WDSP_API PANEL
{
public:
    int run;
    int size;
    double* in;
    double* out;
    double gain1;
    double gain2I;
    double gain2Q;
    int inselect;
    int copy;

    static PANEL* create_panel (int run, int size, double* in, double* out, double gain1, double gain2I, double gain2Q, int inselect, int copy);
    static void destroy_panel (PANEL *a);
    static void flush_panel (PANEL *a);
    static void xpanel (PANEL *a);
    static void setBuffers_panel (PANEL *a, double* in, double* out);
    static void setSamplerate_panel (PANEL *a, int rate);
    static void setSize_panel (PANEL *a, int size);
    // RXA Properties
    static void SetPanelRun (RXA& rxa, int run);
    static void SetPanelSelect (RXA& rxa, int select);
    static void SetPanelGain1 (RXA& rxa, double gain);
    static void SetPanelGain2 (RXA& rxa, double gainI, double gainQ);
    static void SetPanelPan (RXA& rxa, double pan);
    static void SetPanelCopy (RXA& rxa, int copy);
    static void SetPanelBinaural (RXA& rxa, int bin);
    // TXA Properties
    static void SetPanelRun (TXA& txa, int run);
    static void SetPanelGain1 (TXA& txa, double gain);
    static void SetPanelSelect (TXA& txa, int select);
};

} // namespace WDSP

#endif
