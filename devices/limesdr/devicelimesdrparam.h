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

#ifndef DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_
#define DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_

#include "lime/LimeSuite.h"

/**
 * This structure is owned by each of the parties sharing the same physical device
 * It allows exchange of information on the common resources
 */
struct DeviceLimeSDRParams
{
    lms_device_t *m_dev;         //!< device handle if the party has ownership else 0
    lms_range_t  m_lpfRangeRx;   //!< Low pass filter range information (Rx side)
    lms_range_t  m_lpfRangeTx;   //!< Low pass filter range information (Tx side)
    lms_range_t  m_loRangeRx[2]; //!< LO range for Rx
    lms_range_t  m_loRangeTx[2]; //!< LO range for Tx
    lms_range_t  m_srRangeRx[2]; //!< ADC sample rate range
    lms_range_t  m_srRangeTx[2]; //!< DAC sample rate range

    DeviceLimeSDRParams() :
        m_dev(0)
    {
    }
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_ */
