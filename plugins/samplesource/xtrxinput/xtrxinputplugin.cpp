///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#include "xtrxinputplugin.h"

#include <QtPlugin>

#include <regex>
#include <string>

#include "xtrx_api.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "device/devicesourceapi.h"

#ifdef SERVER_MODE
#include "xtrxinput.h"
#else
#include "xtrxinputgui.h"
#endif

const PluginDescriptor XTRXInputPlugin::m_pluginDescriptor = {
    QString("XTRX Input"),
    QString("4.5.2"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString XTRXInputPlugin::m_hardwareID = "XTRX";
const QString XTRXInputPlugin::m_deviceTypeID = XTRX_DEVICE_TYPE_ID;

XTRXInputPlugin::XTRXInputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& XTRXInputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void XTRXInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices XTRXInputPlugin::enumSampleSources()
{
    SamplingDevices result;
    xtrx_device_info_t devs[32];
    int res = xtrx_discovery(devs, 32);
    int i;
    for (i = 0; i < res; i++) {
        DeviceXTRXParams XTRXParams;
        for (unsigned int j = 0; j < XTRXParams.m_nbRxChannels; j++)
        {
            qDebug("XTRXInputPlugin::enumSampleSources: device #%d channel %u: %s", i, j, devs[i].uniqname);
            QString displayedName(QString("XTRX[%1:%2] %3").arg(i).arg(j).arg(devs[i].uniqname));
            result.append(SamplingDevice(displayedName,
                                         m_hardwareID,
                                         m_deviceTypeID,
                                         QString(devs[i].uniqname),
                                         i,
                                         PluginInterface::SamplingDevice::PhysicalDevice,
                                         true,
                                         XTRXParams.m_nbRxChannels,
                                         j));
        }
    }
    return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* XTRXInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* XTRXInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        XTRXInputGUI* gui = new XTRXInputGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSource *XTRXInputPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        XTRXInput* input = new XTRXInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
