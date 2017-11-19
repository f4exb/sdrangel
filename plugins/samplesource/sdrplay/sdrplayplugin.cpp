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
#include <QAction>
#include <mirisdr.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "sdrplaygui.h"
#include "sdrplayplugin.h"
#include <device/devicesourceapi.h>

const PluginDescriptor SDRPlayPlugin::m_pluginDescriptor = {
    QString("SDRPlay RSP1 Input"),
    QString("3.8.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString SDRPlayPlugin::m_hardwareID = "SDRplay1";
const QString SDRPlayPlugin::m_deviceTypeID = SDRPLAY_DEVICE_TYPE_ID;

SDRPlayPlugin::SDRPlayPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& SDRPlayPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SDRPlayPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices SDRPlayPlugin::enumSampleSources()
{
	SamplingDevices result;
	int count = mirisdr_get_device_count();

	char vendor[256];
	char product[256];
	char serial[256];

	for(int i = 0; i < count; i++)
	{
		vendor[0] = '\0';
		product[0] = '\0';
		serial[0] = '\0';

		if (mirisdr_get_device_usb_strings((uint32_t)i, vendor, product, serial) != 0)
		{
			continue;
		}

		qDebug("SDRPlayPlugin::enumSampleSources: found %s:%s (%s)", vendor, product, serial);
		QString displayedName(QString("SDRPlay[%1] %2").arg(i).arg(serial));

		result.append(SamplingDevice(displayedName,
		        m_hardwareID,
				m_deviceTypeID,
				QString(serial),
				i,
				PluginInterface::SamplingDevice::PhysicalDevice,
				true,
				1,
				0));
	}

    return result;
}

PluginInstanceGUI* SDRPlayPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        SDRPlayGui* gui = new SDRPlayGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}

DeviceSampleSource *SDRPlayPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        SDRPlayInput* input = new SDRPlayInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

