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

#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdrinputplugin.h"

#ifdef SERVER_MODE
#include "soapysdrinput.h"
#else
#include "soapysdrinputgui.h"
#endif

const PluginDescriptor SoapySDRInputPlugin::m_pluginDescriptor = {
    QString("SoapySDR Input"),
    QString("4.5.2"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString SoapySDRInputPlugin::m_hardwareID = "SoapySDR";
const QString SoapySDRInputPlugin::m_deviceTypeID = SOAPYSDRINPUT_DEVICE_TYPE_ID;

SoapySDRInputPlugin::SoapySDRInputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& SoapySDRInputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SoapySDRInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices SoapySDRInputPlugin::enumSampleSources()
{
    SamplingDevices result;
    DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
    const std::vector<DeviceSoapySDRScan::SoapySDRDeviceEnum>& devicesEnumeration = deviceSoapySDR.getDevicesEnumeration();
    qDebug("SoapySDRInputPlugin::enumSampleSources: %lu SoapySDR devices. Enumerate these with Rx channel(s):", devicesEnumeration.size());
    std::vector<DeviceSoapySDRScan::SoapySDRDeviceEnum>::const_iterator it = devicesEnumeration.begin();

    for (int idev = 0; it != devicesEnumeration.end(); ++it, idev++)
    {
        unsigned int nbRxChannels = it->m_nbRx;

        for (unsigned int ichan = 0; ichan < nbRxChannels; ichan++)
        {
            QString displayedName(QString("SoapySDR[%1:%2] %3").arg(idev).arg(ichan).arg(it->m_label));
            QString serial(QString("%1-%2").arg(it->m_driverName).arg(it->m_sequence));
            qDebug("SoapySDRInputPlugin::enumSampleSources: device #%d (%s) serial %s channel %u",
                    idev, it->m_label.toStdString().c_str(), serial.toStdString().c_str(), ichan);
            result.append(SamplingDevice(displayedName,
                    m_hardwareID,
                    m_deviceTypeID,
                    serial,
                    idev,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleRx,
                    nbRxChannels,
                    ichan));
        }
    }

    return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* SoapySDRInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
PluginInstanceGUI* SoapySDRInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        SoapySDRInputGui* gui = new SoapySDRInputGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSource *SoapySDRInputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        SoapySDRInput *input = new SoapySDRInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}





