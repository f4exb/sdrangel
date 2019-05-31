///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifdef SERVER_MODE
#include "remoteoutput.h"
#else
#include "remoteoutputgui.h"
#endif
#include "remoteoutputplugin.h"

const PluginDescriptor RemoteOutputPlugin::m_pluginDescriptor = {
	QString("Remote device output"),
	QString("4.5.2"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString RemoteOutputPlugin::m_hardwareID = "RemoteOutput";
const QString RemoteOutputPlugin::m_deviceTypeID = REMOTEOUTPUT_DEVICE_TYPE_ID;

RemoteOutputPlugin::RemoteOutputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& RemoteOutputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void RemoteOutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices RemoteOutputPlugin::enumSampleSinks()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "RemoteOutput",
            m_hardwareID,
            m_deviceTypeID,
            QString::null,
            0,
            PluginInterface::SamplingDevice::BuiltInDevice,
            PluginInterface::SamplingDevice::StreamSingleTx,
            1,
            0));

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* RemoteOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    return 0;
}
#else
PluginInstanceGUI* RemoteOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sinkId == m_deviceTypeID)
	{
	    RemoteOutputSinkGui* gui = new RemoteOutputSinkGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSink* RemoteOutputPlugin::createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        RemoteOutput* output = new RemoteOutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }

}
