///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include "device/devicesourceapi.h"

#ifdef SERVER_MODE
#include "remoteinput.h"
#else
#include "remoteinputgui.h"
#endif
#include "remoteinputplugin.h"

const PluginDescriptor RemoteInputPlugin::m_pluginDescriptor = {
	QString("Remote device input"),
	QString("4.5.6"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString RemoteInputPlugin::m_hardwareID = "RemoteInput";
const QString RemoteInputPlugin::m_deviceTypeID = REMOTEINPUT_DEVICE_TYPE_ID;

RemoteInputPlugin::RemoteInputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& RemoteInputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void RemoteInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices RemoteInputPlugin::enumSampleSources()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "RemoteInput",
            m_hardwareID,
            m_deviceTypeID,
            QString::null,
            0,
            PluginInterface::SamplingDevice::BuiltInDevice,
            true,
            1,
            0));

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* RemoteInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* RemoteInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID)
	{
		RemoteInputGui* gui = new RemoteInputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *RemoteInputPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        RemoteInput* input = new RemoteInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
