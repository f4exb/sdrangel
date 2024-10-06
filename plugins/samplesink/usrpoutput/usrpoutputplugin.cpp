///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <string>

#include "plugin/pluginapi.h"
#include "usrp/deviceusrp.h"

#ifdef SERVER_MODE
#include "usrpoutput.h"
#else
#include "usrpoutputgui.h"
#endif
#include "usrpoutputplugin.h"
#include "usrpoutputwebapiadapter.h"

const PluginDescriptor USRPOutputPlugin::m_pluginDescriptor = {
    QStringLiteral("USRP"),
    QStringLiteral("URSP Output"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE and Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "USRP";
const char* const USRPOutputPlugin::m_deviceTypeID = USRPOUTPUT_DEVICE_TYPE_ID;

USRPOutputPlugin::USRPOutputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& USRPOutputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void USRPOutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

void USRPOutputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceUSRP::enumOriginDevices(m_hardwareID, originDevices);
	listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices USRPOutputPlugin::enumSampleSinks(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            for (int j = 0; j < it->nbTxStreams; j++)
            {
                qDebug("USRPOutputPlugin::enumSampleSinks: device #%d channel %u: %s", it->sequence, j, qPrintable(it->serial));
                QString displayedName = it->displayableName;
                displayedName.replace(QString("$1]"), QString("%1]").arg(j));
                result.append(SamplingDevice(
                    displayedName,
                    it->hardwareId,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleTx,
                    it->nbTxStreams,
                    j
                ));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* USRPOutputPlugin::createSampleSinkPluginInstanceGUI(
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
DeviceGUI* USRPOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sinkId == m_deviceTypeID)
    {
        USRPOutputGUI* gui = new USRPOutputGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSink* USRPOutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        USRPOutput* output = new USRPOutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *USRPOutputPlugin::createDeviceWebAPIAdapter() const
{
    return new USRPOutputWebAPIAdapter();
}
