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
#include "testmi.h"
#else
#include "testmigui.h"
#endif
#include "testmiplugin.h"

const PluginDescriptor TestMIPlugin::m_pluginDescriptor = {
	QString("Test Multiple Input"),
	QString("4.8.1"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString TestMIPlugin::m_hardwareID = "TestMI";
const QString TestMIPlugin::m_deviceTypeID = TESTMI_DEVICE_TYPE_ID;

TestMIPlugin::TestMIPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& TestMIPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void TestMIPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleMIMO(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices TestMIPlugin::enumSampleMIMO()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "TestMI",
            m_hardwareID,
            m_deviceTypeID,
            QString::null,
            0,
            PluginInterface::SamplingDevice::BuiltInDevice,
            PluginInterface::SamplingDevice::StreamAny,
            1,
            0));

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* TestMIPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* TestMIPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID) {
		TestMIGui* gui = new TestMIGui(deviceUISet);
		*widget = gui;
		return gui;
	} else {
		return 0;
	}
}
#endif

DeviceSampleMIMO *TestMIPlugin::createSampleMIMOPluginInstanceMIMO(const QString& mimoId, DeviceAPI *deviceAPI)
{
    if (mimoId == m_deviceTypeID)
    {
        TestMI* input = new TestMI(deviceAPI);
        return input;
    }
    else
    {
        return nullptr;
    }
}

