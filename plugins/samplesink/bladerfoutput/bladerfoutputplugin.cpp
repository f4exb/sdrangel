///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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
#include <QAction>
#include <libbladeRF.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include <device/devicesourceapi.h>

#include "bladerfoutputplugin.h"
#include "bladerfoutputgui.h"

const PluginDescriptor BladerfOutputPlugin::m_pluginDescriptor = {
	QString("BladeRF Output"),
	QString("3.9.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString BladerfOutputPlugin::m_hardwareID = "BladeRF";
const QString BladerfOutputPlugin::m_deviceTypeID = BLADERFOUTPUT_DEVICE_TYPE_ID;

BladerfOutputPlugin::BladerfOutputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& BladerfOutputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void BladerfOutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices BladerfOutputPlugin::enumSampleSinks()
{
	SamplingDevices result;
	struct bladerf_devinfo *devinfo = 0;

	int count = bladerf_get_device_list(&devinfo);

	for(int i = 0; i < count; i++)
	{
		QString displayedName(QString("BladeRF[%1] %2").arg(devinfo[i].instance).arg(devinfo[i].serial));

		result.append(SamplingDevice(displayedName,
		        m_hardwareID,
				m_deviceTypeID,
				QString(devinfo[i].serial),
				i,
				PluginInterface::SamplingDevice::PhysicalDevice,
				false,
				1,
				0));
	}

	if (devinfo)
	{
		bladerf_free_device_list(devinfo); // Valgrind memcheck
	}

	return result;
}

PluginInstanceGUI* BladerfOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sinkId == m_deviceTypeID)
	{
		BladerfOutputGui* gui = new BladerfOutputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}

DeviceSampleSink* BladerfOutputPlugin::createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceSinkAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        BladerfOutput* output = new BladerfOutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }

}
