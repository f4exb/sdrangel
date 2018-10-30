///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_SOAPYSDR_DEVICESOAPYSDR_H_
#define DEVICES_SOAPYSDR_DEVICESOAPYSDR_H_

#include <stdint.h>
#include <SoapySDR/Device.hpp>

#include "export.h"
#include "devicesoapysdrscan.h"

class DEVICES_API DeviceSoapySDR
{
public:
    static DeviceSoapySDR& instance();
    SoapySDR::Device *openSoapySDR(uint32_t sequence);
    void closeSoapySdr(SoapySDR::Device *device);

    uint32_t getNbDevices() const { return m_scanner.getNbDevices(); }
    const std::vector<DeviceSoapySDRScan::SoapySDRDeviceEnum>& getDevicesEnumeration() const { return m_scanner.getDevicesEnumeration(); }

protected:
    DeviceSoapySDR();
    DeviceSoapySDR(const DeviceSoapySDR&) {}
    DeviceSoapySDR& operator=(const DeviceSoapySDR& other __attribute__((unused))) { return *this; }
    ~DeviceSoapySDR();

private:
    SoapySDR::Device *openopenSoapySDRFromSequence(uint32_t sequence);
    DeviceSoapySDRScan m_scanner;
};

#endif /* DEVICES_SOAPYSDR_DEVICESOAPYSDR_H_ */
