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

#include "../bladerfinput/bladerfinputplugin.h"

#include <QtPlugin>
#include <QAction>
#include <libbladeRF.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include <device/devicesourceapi.h>

#include "bladerfinputgui.h"

const PluginDescriptor BlderfInputPlugin::m_pluginDescriptor = {
	QString("BladerRF Input"),
	QString("3.8.2"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString BlderfInputPlugin::m_hardwareID = "BladeRF";
const QString BlderfInputPlugin::m_deviceTypeID = BLADERF_DEVICE_TYPE_ID;

BlderfInputPlugin::BlderfInputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& BlderfInputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void BlderfInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices BlderfInputPlugin::enumSampleSources()
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
				true,
				1,
				0));
	}

	if (devinfo)
	{
		bladerf_free_device_list(devinfo); // Valgrind memcheck
	}

	return result;
}

PluginInstanceGUI* BlderfInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID)
	{
		BladerfInputGui* gui = new BladerfInputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}

DeviceSampleSource *BlderfInputPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        BladerfInput *input = new BladerfInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

