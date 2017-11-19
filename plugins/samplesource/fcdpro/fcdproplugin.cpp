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
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "fcdproplugin.h"

#include <device/devicesourceapi.h>

#include "fcdprogui.h"
#include "fcdtraits.h"

const PluginDescriptor FCDProPlugin::m_pluginDescriptor = {
	QString(fcd_traits<Pro>::pluginDisplayedName),
	QString(fcd_traits<Pro>::pluginVersion),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

FCDProPlugin::FCDProPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FCDProPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FCDProPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(fcd_traits<Pro>::interfaceIID, this);
}

PluginInterface::SamplingDevices FCDProPlugin::enumSampleSources()
{
	SamplingDevices result;

	int i = 0;
	struct hid_device_info *device_info = hid_enumerate(fcd_traits<Pro>::vendorId, fcd_traits<Pro>::productId);

	while (device_info != 0)
	{
		QString serialNumber = QString::fromWCharArray(device_info->serial_number);
		QString displayedName(QString("%1[%2] %3").arg(fcd_traits<Pro>::displayedName).arg(i).arg(serialNumber));

		result.append(SamplingDevice(displayedName,
		        fcd_traits<Pro>::hardwareID,
		        fcd_traits<Pro>::interfaceIID,
				serialNumber,
				i,
				PluginInterface::SamplingDevice::PhysicalDevice,
				true,
				1,
				0));

		device_info = device_info->next;
		i++;
	}

	return result;
}

PluginInstanceGUI* FCDProPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == fcd_traits<Pro>::interfaceIID)
	{
		FCDProGui* gui = new FCDProGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}

DeviceSampleSource *FCDProPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == fcd_traits<Pro>::interfaceIID)
    {
        FCDProInput* input = new FCDProInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
