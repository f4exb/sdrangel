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

#include <SoapySDR/Version.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Device.hpp>

#include <QDebug>
#include <QString>

#include "devicesoapysdrscan.h"

void DeviceSoapySDRScan::scan()
{
    qDebug("DeviceSoapySDRScan::scan: Lib Version: v%s", SoapySDR::getLibVersion().c_str());
    qDebug("DeviceSoapySDRScan::scan: API Version: v%s", SoapySDR::getAPIVersion().c_str());
    qDebug("DeviceSoapySDRScan::scan: ABI Version: v%s", SoapySDR::getABIVersion().c_str());
    qDebug("DeviceSoapySDRScan::scan: Install root: %s", SoapySDR::getRootPath().c_str());

    const std::vector<std::string>& modules = SoapySDR::listModules();

    for (const auto &it : modules)
    {
        const auto &errMsg = SoapySDR::loadModule(it);

        if (not errMsg.empty()) {
            qWarning("DeviceSoapySDRScan::scan: cannot load module %s: %s", it.c_str(), errMsg.c_str());
        } else {
            qDebug("DeviceSoapySDRScan::scan: loaded module: %s", it.c_str());
        }
    }

    SoapySDR::FindFunctions findFunctions = SoapySDR::Registry::listFindFunctions();
    SoapySDR::Kwargs kwargs;
    m_deviceEnums.clear();

    for (const auto &it : findFunctions) // for each driver
    {
        qDebug("DeviceSoapySDRScan::scan: driver: %s", it.first.c_str());
        kwargs["driver"] = it.first;

        SoapySDR::KwargsList kwargsList = SoapySDR::Device::enumerate(kwargs);
        SoapySDR::KwargsList::const_iterator kit = kwargsList.begin();

        for (int deviceSeq = 0; kit != kwargsList.end(); ++kit, deviceSeq++) // for each device
        {
            m_deviceEnums.push_back(SoapySDRDeviceEnum());
            m_deviceEnums.back().m_driverName = QString(it.first.c_str());
            m_deviceEnums.back().m_sequence = deviceSeq;

            // collect identification information

            SoapySDR::Kwargs::const_iterator kargIt;

            if ((kargIt = kit->find("label")) != kit->end()) {
                m_deviceEnums.back().m_label = QString("%1: %2").arg(m_deviceEnums.back().m_driverName).arg(kargIt->second.c_str());
            } else { // if no label is registered for this device then create a label with the driver name and sequence
                m_deviceEnums.back().m_label = QString("%1-%2").arg(m_deviceEnums.back().m_driverName).arg(deviceSeq);
            }

            if ((kargIt = kit->find("serial")) != kit->end())
            {
                m_deviceEnums.back().m_idKey = QString(kargIt->first.c_str());
                m_deviceEnums.back().m_idValue = QString(kargIt->second.c_str());
            }
            else if ((kargIt = kit->find("device_id")) != kit->end())
            {
                m_deviceEnums.back().m_idKey = QString(kargIt->first.c_str());
                m_deviceEnums.back().m_idValue = QString(kargIt->second.c_str());
            }
            else if ((kargIt = kit->find("addr")) != kit->end())
            {
                m_deviceEnums.back().m_idKey = QString(kargIt->first.c_str());
                m_deviceEnums.back().m_idValue = QString(kargIt->second.c_str());
            }

            qDebug("DeviceSoapySDRScan::scan: %s #%u %s id: %s=%s",
                    m_deviceEnums.back().m_driverName.toStdString().c_str(),
                    deviceSeq,
                    m_deviceEnums.back().m_label.toStdString().c_str(),
                    m_deviceEnums.back().m_idKey.toStdString().c_str(),
                    m_deviceEnums.back().m_idValue.toStdString().c_str());

            // access the device to get the number of Rx and Tx channels and at the same time probe
            // whether it is available for Soapy

            try
            {
                SoapySDR::Device *device;
                SoapySDR::Kwargs kwargs;
                kwargs["driver"] = m_deviceEnums.back().m_driverName.toStdString();

                if (m_deviceEnums.back().m_idKey.size() > 0)  {
                    kwargs[m_deviceEnums.back().m_idKey.toStdString()] = m_deviceEnums.back().m_idValue.toStdString();
                }

                device =  SoapySDR::Device::make(kwargs);
                m_deviceEnums.back().m_nbRx = device->getNumChannels(SOAPY_SDR_RX);
                m_deviceEnums.back().m_nbTx = device->getNumChannels(SOAPY_SDR_TX);
                qDebug("DeviceSoapySDRScan::scan: %s #%u driver=%s hardware=%s #Rx=%u #Tx=%u",
                        m_deviceEnums.back().m_driverName.toStdString().c_str(),
                        deviceSeq,
                        device->getDriverKey().c_str(),
                        device->getHardwareKey().c_str(),
                        m_deviceEnums.back().m_nbRx,
                        m_deviceEnums.back().m_nbTx);

                SoapySDR::Device::unmake(device);
            }
            catch (const std::exception &ex)
            {
                qWarning("DeviceSoapySDRScan::scan: %s #%u cannot be opened: %s",
                        m_deviceEnums.back().m_driverName.toStdString().c_str(),
                        deviceSeq,
                        ex.what());
                m_deviceEnums.pop_back();
            }
        } // for each device
    }
}
