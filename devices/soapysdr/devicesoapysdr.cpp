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

#include <QStringList>
#include "devicesoapysdr.h"

DeviceSoapySDR::DeviceSoapySDR()
{
    m_scanner.scan();
}

DeviceSoapySDR::~DeviceSoapySDR()
{}

DeviceSoapySDR& DeviceSoapySDR::instance()
{
    static DeviceSoapySDR inst;
    return inst;
}

SoapySDR::Device *DeviceSoapySDR::openSoapySDR(uint32_t sequence, const QString& hardwareUserArguments)
{
    instance();
    return openopenSoapySDRFromSequence(sequence, hardwareUserArguments);
}

void DeviceSoapySDR::closeSoapySdr(SoapySDR::Device *device)
{
    SoapySDR::Device::unmake(device);
}

SoapySDR::Device *DeviceSoapySDR::openopenSoapySDRFromSequence(uint32_t sequence, const QString& hardwareUserArguments)
{
    if (sequence > m_scanner.getNbDevices())
    {
        return 0;
    }
    else
    {
        const DeviceSoapySDRScan::SoapySDRDeviceEnum& deviceEnum = m_scanner.getDevicesEnumeration()[sequence];

        try
        {
            SoapySDR::Kwargs kwargs;
            kwargs["driver"] = deviceEnum.m_driverName.toStdString();

            if (hardwareUserArguments.size() > 0)
            {
                QStringList kvArgs = hardwareUserArguments.split(',');

                for (int i = 0; i < kvArgs.size(); i++)
                {
                    QStringList kv = kvArgs.at(i).split('=');

                    if (kv.size() > 1) {
                        kwargs[kv.at(0).toStdString()] = kv.at(1).toStdString();
                    }
                }
            }
            else if (deviceEnum.m_idKey.size() > 0)
            {
                kwargs[deviceEnum.m_idKey.toStdString()] = deviceEnum.m_idValue.toStdString();
            }

            SoapySDR::Kwargs::const_iterator it = kwargs.begin();

            for (; it != kwargs.end(); ++it) {
                qDebug("DeviceSoapySDR::openopenSoapySDRFromSequence: %s=%s", it->first.c_str(), it->second.c_str());
            }

            SoapySDR::Device *device = SoapySDR::Device::make(kwargs);
            return device;
        }
        catch (const std::exception &ex)
        {
            qWarning("DeviceSoapySDR::openopenSoapySDRFromSequence: %s cannot be opened: %s",
                    deviceEnum.m_label.toStdString().c_str(), ex.what());
            return 0;
        }
    }
}

void DeviceSoapySDR::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    const std::vector<DeviceSoapySDRScan::SoapySDRDeviceEnum>& devicesEnumeration = getDevicesEnumeration();
    qDebug("SoapySDROutputPlugin::enumOriginDevices: %lu SoapySDR devices", devicesEnumeration.size());
    std::vector<DeviceSoapySDRScan::SoapySDRDeviceEnum>::const_iterator it = devicesEnumeration.begin();

    for (int idev = 0; it != devicesEnumeration.end(); ++it, idev++)
    {
        QString displayedName(QString("SoapySDR[%1:$1] %2").arg(idev).arg(it->m_label));
        QString serial(QString("%1-%2").arg(it->m_driverName).arg(it->m_sequence));

        originDevices.append(PluginInterface::OriginDevice(
            displayedName,
            hardwareId,
            serial,
            idev, // Sequence
            it->m_nbRx, // nb Rx
            it->m_nbTx  // nb Tx
        ));
    }
}