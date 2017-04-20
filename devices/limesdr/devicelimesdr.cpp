///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <cstdio>
#include <cstring>
#include "devicelimesdr.h"

bool DeviceLimeSDR::setNCOFrequency(lms_device_t *device, bool dir_tx, std::size_t chan, bool enable, float frequency)
{
    if (enable)
    {
        bool positive;
        float_type freqs[LMS_NCO_VAL_COUNT];
        float_type phos[LMS_NCO_VAL_COUNT];

        if (LMS_GetNCOFrequency(device, dir_tx, chan, freqs, phos) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot get NCO frequencies and phases\n");
        }

        if (frequency < 0)
        {
            positive = false;
            frequency = -frequency;
        }
        else
        {
            positive = true;
        }

        freqs[0] = frequency;

        if (LMS_SetNCOFrequency(device, dir_tx, chan, freqs, 0.0f) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot set frequency to %f\n", frequency);
            return false;
        }

        if (LMS_SetNCOIndex(device, dir_tx, chan, 0, !positive) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot set conversion direction %sfreq\n", positive ? "+" : "-");
            return false;
        }

        return true;
    }
    else
    {
        if (LMS_SetNCOIndex(device, dir_tx, chan, LMS_NCO_VAL_COUNT, true) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot disable NCO\n");
            return false;
        }
    }
}


