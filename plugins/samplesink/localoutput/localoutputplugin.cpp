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
#include "localoutput.h"
#else
#include "localoutputgui.h"
#endif
#include "localoutputplugin.h"

const PluginDescriptor LocalOutputPlugin::m_pluginDescriptor = {
	QString("Local device output"),
	QString("4.8.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString LocalOutputPlugin::m_hardwareID = "LocalOutput";
const QString LocalOutputPlugin::m_deviceTypeID = LOCALOUTPUT_DEVICE_TYPE_ID;

LocalOutputPlugin::LocalOutputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& LocalOutputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void LocalOutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices LocalOutputPlugin::enumSampleSinks()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "LocalOutput",
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
PluginInstanceGUI* LocalOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* LocalOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sinkId == m_deviceTypeID)
	{
		LocalOutputGui* gui = new LocalOutputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSink *LocalOutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if (sinkId == m_deviceTypeID)
    {
        LocalOutput* output = new LocalOutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}
