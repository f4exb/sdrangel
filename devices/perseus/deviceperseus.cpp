///////////////////////////////////////////////////////////////////////////////////
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

#include "perseus-sdr.h"
#include "deviceperseus.h"

DevicePerseus::DevicePerseus()
{
	m_nbDevices = perseus_init();
}

DevicePerseus::~DevicePerseus()
{
	perseus_exit();
}

DevicePerseus& DevicePerseus::instance()
{
    static DevicePerseus inst;
    return inst;
}

void DevicePerseus::scan()
{
    // If some firmware was not downloaded at time of enumeration interface will break later
    // so in this case we re initialize the library, clear the scan results and scan again
    if (!internal_scan())
    {
        qDebug("DevicePerseus::scan: re-init library and scan again");
        m_scan.clear();
        perseus_exit();
        m_nbDevices = perseus_init();
        internal_scan();
    }
}

bool DevicePerseus::internal_scan()
{
    return m_scan.scan(m_nbDevices);
}
