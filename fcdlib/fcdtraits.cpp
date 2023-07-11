///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "fcdtraits.h"

const char *fcd_traits<Pro>::alsaDeviceName = "hw:CARD=V10";
const char *fcd_traits<ProPlus>::alsaDeviceName = "hw:CARD=V20";

#if defined(__linux__)
const char *fcd_traits<Pro>::qtDeviceName = "FUNcube_Dongle_V1.0";
const char *fcd_traits<ProPlus>::qtDeviceName = "FUNcube_Dongle_V2.0";
#else
const char *fcd_traits<Pro>::qtDeviceName = "FUNcube Dongle V1.0";
const char *fcd_traits<ProPlus>::qtDeviceName = "FUNcube Dongle V2.0";
#endif

const char *fcd_traits<Pro>::hardwareID = "FCDPro";
const char *fcd_traits<ProPlus>::hardwareID = "FCDPro+";

const char *fcd_traits<Pro>::interfaceIID = "sdrangel.samplesource.fcdpro";
const char *fcd_traits<ProPlus>::interfaceIID = "sdrangel.samplesource.fcdproplus";

const char *fcd_traits<Pro>::displayedName = "FunCube Dongle Pro";
const char *fcd_traits<ProPlus>::displayedName = "FunCube Dongle Pro+";

const char *fcd_traits<Pro>::pluginDisplayedName = "FunCube Pro Input";
const char *fcd_traits<ProPlus>::pluginDisplayedName = "FunCube Pro+ Input";

const char *fcd_traits<Pro>::pluginVersion = "7.15.1";
const char *fcd_traits<ProPlus>::pluginVersion = "7.15.1";

const int64_t fcd_traits<Pro>::loLowLimitFreq = 64000000L;
const int64_t fcd_traits<ProPlus>::loLowLimitFreq = 150000L;

const int64_t fcd_traits<Pro>::loHighLimitFreq = 1700000000L;
const int64_t fcd_traits<ProPlus>::loHighLimitFreq = 2000000000L;
