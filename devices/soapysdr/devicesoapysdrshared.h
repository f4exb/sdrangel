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

#ifndef DEVICES_SOAPYSDR_DEVICESOAPYSDRSHARED_H_
#define DEVICES_SOAPYSDR_DEVICESOAPYSDRSHARED_H_

#include <SoapySDR/Device.hpp>

#include "export.h"
#include "devicesoapysdrparams.h"

class SoapySDRInput;
class SoapySDROutput;

/**
 * Structure shared by a buddy with other buddies
 */
class DEVICES_API DeviceSoapySDRShared
{
public:
    DeviceSoapySDRShared();
    ~DeviceSoapySDRShared();

    SoapySDR::Device *m_device;
    DeviceSoapySDRParams *m_deviceParams;
    int m_channel; //!< allocated channel (-1 if none)
    SoapySDRInput *m_source;
    SoapySDROutput *m_sink;
};


#endif /* DEVICES_SOAPYSDR_DEVICESOAPYSDRSHARED_H_ */
