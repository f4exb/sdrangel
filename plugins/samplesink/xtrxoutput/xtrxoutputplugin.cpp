///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <QtPlugin>

#include <regex>
#include <string>

#include "xtrx_api.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "device/devicesinkapi.h"
#include "xtrx/devicextrxparam.h"

#ifdef SERVER_MODE
#include "xtrxoutput.h"
#else
#include "xtrxoutputgui.h"
#endif
#include "../xtrxoutput/xtrxoutputplugin.h"

const PluginDescriptor XTRXOutputPlugin::m_pluginDescriptor = {
    QString("XTRX Output"),
    QString("4.5.4"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString XTRXOutputPlugin::m_hardwareID = "XTRX";
const QString XTRXOutputPlugin::m_deviceTypeID = XTRXOUTPUT_DEVICE_TYPE_ID;

XTRXOutputPlugin::XTRXOutputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& XTRXOutputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void XTRXOutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices XTRXOutputPlugin::enumSampleSinks()
{
    SamplingDevices result;
    xtrx_device_info_t devs[32];
    int res = xtrx_discovery(devs, 32);
    int i;
    for (i = 0; i < res; i++) {
        DeviceXTRXParams XTRXParams;
        for (unsigned int j = 0; j < XTRXParams.m_nbTxChannels; j++)
        {
            qDebug("XTRXInputPlugin::enumSampleSinks: device #%d channel %u: %s", i, j, devs[i].uniqname);
            QString displayedName(QString("XTRX[%1:%2] %3").arg(i).arg(j).arg(devs[i].uniqname));
            result.append(SamplingDevice(displayedName,
                                         m_hardwareID,
                                         m_deviceTypeID,
                                         QString(devs[i].uniqname),
                                         i,
                                         PluginInterface::SamplingDevice::PhysicalDevice,
                                         false,
                                         XTRXParams.m_nbTxChannels,
                                         j));
        }
    }
    return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* XTRXOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* XTRXOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sinkId == m_deviceTypeID)
    {
        XTRXOutputGUI* gui = new XTRXOutputGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSink* XTRXOutputPlugin::createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceSinkAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        XTRXOutput* output = new XTRXOutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}

