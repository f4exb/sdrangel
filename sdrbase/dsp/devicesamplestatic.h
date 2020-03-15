///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <stdint.h>

#include "export.h"

class SDRBASE_API DeviceSampleStatic
{
public:
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

    typedef enum {
        FSHIFT_STD = 0, // Standard Rx independent
        FSHIFT_TXSYNC   // Follows same scheme as Tx
    } FrequencyShiftScheme;

    static int64_t calculateSourceDeviceCenterFrequency(
            uint64_t centerFrequency,
            int64_t transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode = false
    );

    static int64_t calculateSourceCenterFrequency(
            uint64_t deviceCenterFrequency,
            int64_t transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode = false
    );

    static int calculateSourceFrequencyShift(
            int log2Decim,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme
    );

    static int64_t calculateSinkDeviceCenterFrequency(
            uint64_t centerFrequency,
            int64_t transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            bool transverterMode = false);

    static int64_t calculateSinkCenterFrequency(
            uint64_t deviceCenterFrequency,
            int64_t transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            bool transverterMode = false);

    static int calculateSinkFrequencyShift(
            int log2Interp,
            fcPos_t fcPos,
            uint32_t devSampleRate);
};
