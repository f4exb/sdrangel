///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "device/devicesinkapi.h"

#ifdef SERVER_MODE
#include "filesinkoutput.h"
#else
#include "filesinkgui.h"
#endif
#include "filesinkplugin.h"

const PluginDescriptor FileSinkPlugin::m_pluginDescriptor = {
	QString("File sink output"),
	QString("3.9.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString FileSinkPlugin::m_hardwareID = "FileSink";
const QString FileSinkPlugin::m_deviceTypeID = FILESINK_DEVICE_TYPE_ID;

FileSinkPlugin::FileSinkPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FileSinkPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FileSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices FileSinkPlugin::enumSampleSinks()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "FileSink",
            m_hardwareID,
            m_deviceTypeID,
            QString::null,
            0,
            PluginInterface::SamplingDevice::BuiltInDevice,
            false,
            1,
            0));

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* FileSinkPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* FileSinkPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sinkId == m_deviceTypeID)
	{
		FileSinkGui* gui = new FileSinkGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSink* FileSinkPlugin::createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceSinkAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        FileSinkOutput* output = new FileSinkOutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }

}

