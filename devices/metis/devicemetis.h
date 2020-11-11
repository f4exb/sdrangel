///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_METIS_DEVICEMETIS_H_
#define DEVICES_METIS_DEVICEMETIS_H_

#include "export.h"
#include "devicemetisscan.h"

class DEVICES_API DeviceMetis
{
public:
    static DeviceMetis& instance();
    void scan() { m_scan.scan(); }
    void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices) {
        m_scan.enumOriginDevices(hardwareId, originDevices);
    }
    const DeviceMetisScan::DeviceScan* getDeviceScanAt(unsigned int index) const { return m_scan.getDeviceAt(index); }

protected:
    DeviceMetis();
    ~DeviceMetis();

private:
    DeviceMetisScan m_scan;    
};

#endif // DEVICES_METIS_DEVICEMETIS_H_