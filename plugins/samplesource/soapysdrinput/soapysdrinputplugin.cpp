///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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
#include "soapysdr/devicesoapysdr.h"

#include "soapysdrinputplugin.h"
#include "soapysdrinputwebapiadapter.h"

#ifdef SERVER_MODE
#include "soapysdrinput.h"
#else
#include "soapysdrinputgui.h"
#endif

const PluginDescriptor SoapySDRInputPlugin::m_pluginDescriptor = {
    QStringLiteral("SoapySDR"),
    QStringLiteral("SoapySDR Input"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "SoapySDR";
static constexpr const char* const m_deviceTypeID = SOAPYSDRINPUT_DEVICE_TYPE_ID;

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

void SoapySDRInputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
    deviceSoapySDR.enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices SoapySDRInputPlugin::enumSampleSources(const OriginDevices& originDevices)
{
    SamplingDevices result;

    for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            unsigned int nbRxChannels = it->nbRxStreams;

            for (unsigned int ichan = 0; ichan < nbRxChannels; ichan++)
            {
                QString displayedName = it->displayableName;
                displayedName.replace(QString("$1]"), QString("%1]").arg(ichan));
                qDebug("SoapySDRInputPlugin::enumSampleSources: device #%d serial %s channel %u",
                        it->sequence, it->serial.toStdString().c_str(), ichan);
                result.append(SamplingDevice(
                    displayedName,
                    it->hardwareId,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleRx,
                    nbRxChannels,
                    ichan
                ));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* SoapySDRInputPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* SoapySDRInputPlugin::createSampleSourcePluginInstanceGUI(
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

DeviceWebAPIAdapter *SoapySDRInputPlugin::createDeviceWebAPIAdapter() const
{
    return new SoapySDRInputWebAPIAdapter();
}
