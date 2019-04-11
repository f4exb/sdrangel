///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_XTRX_DEVICEXTRXPARAM_H_
#define DEVICES_XTRX_DEVICEXTRXPARAM_H_

#include "xtrx_api.h"


struct DeviceXTRXParams
{
    uint32_t         m_nbRxChannels; //!< number of Rx channels
    uint32_t         m_nbTxChannels; //!< number of Tx channels

    DeviceXTRXParams();
    ~DeviceXTRXParams();
};

#endif /* DEVICES_XTRX_DEVICEXTRXPARAM_H_ */
