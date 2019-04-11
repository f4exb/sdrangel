///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_HACKRF_DEVICEHACKRFPARAM_H_
#define DEVICES_HACKRF_DEVICEHACKRFPARAM_H_

#include "libhackrf/hackrf.h"

/**
 * This structure is owned by each of the parties sharing the same physical device
 * It allows exchange of information on the common resources
 */
/**
 * This structure is owned by each of the parties sharing the same physical device
 * It allows exchange of information on the common resources
 */
struct DeviceHackRFParams
{
    struct hackrf_device* m_dev; //!< device handle if the party has ownership else 0

    DeviceHackRFParams() :
        m_dev(0)
    {
    }
};



#endif /* DEVICES_HACKRF_DEVICEHACKRFPARAM_H_ */
