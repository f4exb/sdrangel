///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#include <QDebug>
#include "devicextrxparam.h"

bool DeviceXTRXParams::open(const char* deviceStr)
{
    int res;
    qDebug("DeviceXTRXParams::open: serial: %s", (const char *) deviceStr);

    res = xtrx_open(deviceStr, XTRX_O_RESET | 4, &m_dev);

    if (res)
    {
        qCritical() << "DeviceXTRXParams::open: cannot open device " << deviceStr;
        return false;
    }


    return true;
}

void DeviceXTRXParams::close()
{
    if (m_dev)
    {
        xtrx_close(m_dev);
        m_dev = 0;
    }
}
