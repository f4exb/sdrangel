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

#ifndef DEVICES_LIMESDR_DEVICELIMESDR_H_
#define DEVICES_LIMESDR_DEVICELIMESDR_H_

#include "lime/LimeSuite.h"

class DeviceLimeSDR
{
public:
    /** enable or disable NCO. If re-enabled frequency should have been set once */
    static bool enableNCO(lms_device_t *device, bool dir_tx, std::size_t chan, bool enable);
    /** set NCO frequency with positive or negative frequency (deals with up/down convert). Enables NCO */
    static bool setNCOFrequency(lms_device_t *device, bool dir_tx, std::size_t chan, float frequency);
    /** combination of the two like LMS_SetGFIRLPF */
    static bool setNCOFrequency(lms_device_t *device, bool dir_tx, std::size_t chan, bool enable, float frequency);
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDR_H_ */
