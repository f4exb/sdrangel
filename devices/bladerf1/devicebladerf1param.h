///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2017 Edouard Griffiths, F4EXB                              //
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

#ifndef DEVICES_BLADERF1_DEVICEBLADERF1PARAM_H_
#define DEVICES_BLADERF1_DEVICEBLADERF1PARAM_H_

#include <libbladeRF.h>

/**
 * This structure is owned by each of the parties sharing the same physical device
 * It allows exchange of information on the common resources
 */
struct DeviceBladeRF1Params
{
    struct bladerf *m_dev; //!< device handle if the party has ownership else 0
    bool m_xb200Attached;  //!< true if XB200 is attached and owned by the party

    DeviceBladeRF1Params() :
        m_dev(0),
        m_xb200Attached(false)
    {
    }
};

#endif /* DEVICES_BLADERF1_DEVICEBLADERF1PARAM_H_ */
