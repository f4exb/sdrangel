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

bool DeviceLimeSDR::enableNCO(lms_device_t *device, bool dir_tx, std::size_t chan, bool enable)
{
    if (LMS_WriteParam(device, LMS7param(MAC), chan+1) < 0)
    {
        fprintf(stderr, "DeviceLimeSDR::enableNCO: cannot address channel #%lu\n", chan);
        return false;
    }

    if (dir_tx)
    {
        if (LMS_WriteParam(device, LMS7param(CMIX_BYP_RXTSP), enable ? 0 : 1) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::enableNCO: cannot %s Rx NCO\n", enable ? "enable" : "disable");
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        if (LMS_WriteParam(device, LMS7param(CMIX_BYP_TXTSP), enable ? 0 : 1) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::enableNCO: cannot %s Tx NCO\n", enable ? "enable" : "disable");
            return false;
        }
        else
        {
            return true;
        }
    }
}

bool DeviceLimeSDR::setNCOFrequency(lms_device_t *device, bool dir_tx, std::size_t chan, float frequency)
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

    if (LMS_SetNCOIndex(device, dir_tx, chan, 0, positive) < 0) // TODO: verify positive = downconvert ?
    {
        fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot set conversion direction %s\n", positive ? "down" : "up");
        return false;
    }

    return true;
}

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

        if (LMS_SetNCOIndex(device, dir_tx, chan, 0, positive) < 0) // TODO: verify positive = downconvert ?
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot set conversion direction %s\n", positive ? "down" : "up");
            return false;
        }

        return true;
    }
    else
    {
        if (LMS_WriteParam(device, LMS7param(MAC), chan+1) < 0)
        {
            fprintf(stderr, "DeviceLimeSDR::setNCOFrequency: cannot address channel #%lu\n", chan);
            return false;
        }

        if (dir_tx)
        {
            if (LMS_WriteParam(device, LMS7param(CMIX_BYP_RXTSP), 1) < 0)
            {
                fprintf(stderr, "DeviceLimeSDR::enableNCO: cannot disable Rx NCO on channel %lu\n", chan);
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            if (LMS_WriteParam(device, LMS7param(CMIX_BYP_TXTSP), 1) < 0)
            {
                fprintf(stderr, "DeviceLimeSDR::enableNCO: cannot disable Tx NCO on channel %lu\n", chan);
                return false;
            }
            else
            {
                return true;
            }
        }
    }
}


