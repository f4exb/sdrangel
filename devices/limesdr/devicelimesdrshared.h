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

#ifndef DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_
#define DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_

#include <cstddef>
#include "devicelimesdrparam.h"

/**
 * Structure shared by a buddy with other buddies
 */
struct DeviceLimeSDRShared
{
    DeviceLimeSDRParams *m_deviceParams; //!< unique hardware device parameters
    std::size_t         m_channel;       //!< logical device channel number (-1 if none)
    void                *m_thread;       //!< anonymous pointer that will hold the thread address if started else 0

    DeviceLimeSDRShared() :
        m_deviceParams(0),
        m_channel(-1),
        m_thread(0)
    {}

    ~DeviceLimeSDRShared()
    {}
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_ */
