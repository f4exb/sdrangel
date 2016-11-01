///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_DSP_HBFILTERTRAITS_H_
#define SDRBASE_DSP_HBFILTERTRAITS_H_

#include <stdint.h>

// uses Q1.14 format internally, input and output are S16

/*
 * supported filter orders: 96, 80, 64, 48, 32
 * any usage of another value will be prevented by compilation errors
 */
template<uint32_t HBFilterOrder>
struct HBFIRFilterTraits
{
};

template<>
struct HBFIRFilterTraits<32>
{
    static const int32_t hbOrder = 32;
    static const int32_t hbShift = 14;
    static const int16_t hbMod[32+6];
    static const int32_t hbCoeffs[8];
};

template<>
struct HBFIRFilterTraits<48>
{
    static const int32_t hbOrder = 48;
    static const int32_t hbShift = 14;
    static const int16_t hbMod[48+6];
    static const int32_t hbCoeffs[12];
};

template<>
struct HBFIRFilterTraits<64>
{
    static const int32_t hbOrder = 64;
    static const int32_t hbShift = 14;
    static const int16_t hbMod[64+6];
    static const int32_t hbCoeffs[16];
};

template<>
struct HBFIRFilterTraits<80>
{
    static const int32_t hbOrder = 80;
    static const int32_t hbShift = 14;
    static const int16_t hbMod[80+6];
    static const int32_t hbCoeffs[20];
};

template<>
struct HBFIRFilterTraits<96>
{
    static const int32_t hbOrder = 96;
    static const int32_t hbShift = 16;
    static const int16_t hbMod[96+6];
    static const int32_t hbCoeffs[24];
};

#endif /* SDRBASE_DSP_HBFILTERTRAITS_H_ */
