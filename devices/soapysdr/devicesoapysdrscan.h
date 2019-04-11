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

#ifndef DEVICES_SOAPYSDR_DEVICESOAPYSDRSCAN_H_
#define DEVICES_SOAPYSDR_DEVICESOAPYSDRSCAN_H_

#include <vector>

#include <QString>

#include "export.h"

class DEVICES_API DeviceSoapySDRScan
{
public:
    struct SoapySDRDeviceEnum
    {
        QString m_driverName;
        uint32_t m_sequence; //!< device sequence for this driver
        QString m_label;     //!< the label key for display should always be present
        QString m_idKey;     //!< key to uniquely identify device
        QString m_idValue;   //!< value for the above key
        uint32_t m_nbRx;
        uint32_t m_nbTx;

        SoapySDRDeviceEnum() : m_sequence(0), m_nbRx(0), m_nbTx(0)
        {}
    };

    void scan();
    uint32_t getNbDevices() const { return m_deviceEnums.size(); }
    const std::vector<SoapySDRDeviceEnum>& getDevicesEnumeration() const { return m_deviceEnums; }

private:
    std::vector<SoapySDRDeviceEnum> m_deviceEnums;
};


#endif /* DEVICES_SOAPYSDR_DEVICESOAPYSDRSCAN_H_ */
