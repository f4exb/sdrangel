///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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
#include <device/devicesourceapi.h>

#ifdef SERVER_MODE
#include "testsourceinput.h"
#else
#include "testsourcegui.h"
#endif
#include "testsourceplugin.h"

const PluginDescriptor TestSourcePlugin::m_pluginDescriptor = {
	QString("Test Source input"),
	QString("3.11.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString TestSourcePlugin::m_hardwareID = "TestSource";
const QString TestSourcePlugin::m_deviceTypeID = TESTSOURCE_DEVICE_TYPE_ID;

TestSourcePlugin::TestSourcePlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& TestSourcePlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void TestSourcePlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices TestSourcePlugin::enumSampleSources()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "TestSource",
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
PluginInstanceGUI* TestSourcePlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* TestSourcePlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID) {
		TestSourceGui* gui = new TestSourceGui(deviceUISet);
		*widget = gui;
		return gui;
	} else {
		return 0;
	}
}
#endif

DeviceSampleSource *TestSourcePlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        TestSourceInput* input = new TestSourceInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

