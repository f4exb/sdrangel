///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include "localinput.h"
#else
#include "localinputgui.h"
#endif
#include "localinputplugin.h"

const PluginDescriptor LocalInputPlugin::m_pluginDescriptor = {
	QString("Local device input"),
	QString("4.6.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString LocalInputPlugin::m_hardwareID = "LocalInput";
const QString LocalInputPlugin::m_deviceTypeID = LOCALINPUT_DEVICE_TYPE_ID;

LocalInputPlugin::LocalInputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& LocalInputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void LocalInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices LocalInputPlugin::enumSampleSources()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "LocalInput",
            m_hardwareID,
            m_deviceTypeID,
            QString::null,
            0,
            PluginInterface::SamplingDevice::BuiltInDevice,
            PluginInterface::SamplingDevice::StreamSingleRx,
            1,
            0));

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* LocalInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    return 0;
}
#else
PluginInstanceGUI* LocalInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID)
	{
		LocalInputGui* gui = new LocalInputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *LocalInputPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        LocalInput* input = new LocalInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
