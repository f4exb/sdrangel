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

#ifndef FCDLIB_FCDTRAITS_H_
#define FCDLIB_FCDTRAITS_H_

#include <inttypes.h>

#include "export.h"

typedef enum
{
    Pro,
    ProPlus
} fcd_type;

template <fcd_type FCDType>
struct FCDLIB_API fcd_traits
{
	static const uint16_t vendorId = 0x0;
	static const uint16_t productId = 0x0;
	static const int sampleRate = 48000;
	static const int convBufSize = (1<<11);
	static const char *alsaDeviceName;
	static const char *qtDeviceName;
    static const char *hardwareID;
    static const char *interfaceIID;
    static const char *displayedName;
    static const char *pluginDisplayedName;
    static const char *pluginVersion;
    static const int64_t loLowLimitFreq;
    static const int64_t loHighLimitFreq;
};

template<>
struct FCDLIB_API fcd_traits<Pro>
{
	static const uint16_t vendorId = 0x04D8;
	static const uint16_t productId = 0xFB56;
	static const int sampleRate = 96000;
	static const int convBufSize = (1<<11);
	static const char *alsaDeviceName;
    static const char *qtDeviceName;
    static const char *hardwareID;
    static const char *interfaceIID;
    static const char *displayedName;
    static const char *pluginDisplayedName;
    static const char *pluginVersion;
    static const int64_t loLowLimitFreq;
    static const int64_t loHighLimitFreq;
};

template<>
struct FCDLIB_API fcd_traits<ProPlus>
{
	static const uint16_t vendorId = 0x04D8;
	static const uint16_t productId = 0xFB31;
	static const int sampleRate = 192000;
	static const int convBufSize = (1<<10);
	static const char *alsaDeviceName;
    static const char *qtDeviceName;
    static const char *hardwareID;
    static const char *interfaceIID;
    static const char *displayedName;
    static const char *pluginDisplayedName;
    static const char *pluginVersion;
    static const int64_t loLowLimitFreq;
    static const int64_t loHighLimitFreq;
};

template <fcd_type FCDType> const char *fcd_traits<FCDType>::alsaDeviceName = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::hardwareID = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::interfaceIID = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::displayedName = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::pluginDisplayedName = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::pluginVersion = "---";

#endif /* FCDLIB_FCDTRAITS_H_ */
