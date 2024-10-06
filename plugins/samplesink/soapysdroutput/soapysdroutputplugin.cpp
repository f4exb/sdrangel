///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include "soapysdroutputplugin.h"
#include "soapysdroutputwebapiadapter.h"

#ifdef SERVER_MODE
#include "soapysdroutput.h"
#else
#include "soapysdroutputgui.h"
#endif

const PluginDescriptor SoapySDROutputPlugin::m_pluginDescriptor = {
    QStringLiteral("SoapySDR"),
    QStringLiteral("SoapySDR Output"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "SoapySDR";
static constexpr const char* const m_deviceTypeID = SOAPYSDROUTPUT_DEVICE_TYPE_ID;

SoapySDROutputPlugin::SoapySDROutputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& SoapySDROutputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SoapySDROutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

void SoapySDROutputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
    deviceSoapySDR.enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices SoapySDROutputPlugin::enumSampleSinks(const OriginDevices& originDevices)
{
    SamplingDevices result;

    for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            unsigned int nbTxChannels = it->nbTxStreams;

            for (unsigned int ichan = 0; ichan < nbTxChannels; ichan++)
            {
                QString displayedName = it->displayableName;
                displayedName.replace(QString("$1]"), QString("%1]").arg(ichan));
                qDebug("SoapySDROutputPlugin::enumSampleSinks: device #%d serial %s channel %u",
                        it->sequence, it->serial.toStdString().c_str(), ichan);
                result.append(SamplingDevice(
                    displayedName,
                    it->hardwareId,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleTx,
                    nbTxChannels,
                    ichan
                ));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* SoapySDROutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sinkId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
DeviceGUI* SoapySDROutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sinkId == m_deviceTypeID)
    {
        SoapySDROutputGui* gui = new SoapySDROutputGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSink* SoapySDROutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        SoapySDROutput* output = new SoapySDROutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *SoapySDROutputPlugin::createDeviceWebAPIAdapter() const
{
    return new SoapySDROutputWebAPIAdapter();
}
