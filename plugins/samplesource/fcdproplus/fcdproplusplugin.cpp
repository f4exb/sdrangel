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
#include "device/deviceapi.h"
#include "util/simpleserializer.h"
#include "fcdproplusplugin.h"
#include "fcdproplusgui.h"
#include "fcdtraits.h"

const PluginDescriptor FCDProPlusPlugin::m_pluginDescriptor = {
	QString(fcd_traits<ProPlus>::pluginDisplayedName),
	QString(fcd_traits<ProPlus>::pluginVersion),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString FCDProPlusPlugin::m_deviceTypeID = FCDPROPLUS_DEVICE_TYPE_ID;

FCDProPlusPlugin::FCDProPlusPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FCDProPlusPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FCDProPlusPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource(fcd_traits<ProPlus>::interfaceIID, this);
}

PluginInterface::SampleSourceDevices FCDProPlusPlugin::enumSampleSources()
{
	SampleSourceDevices result;

	int i = 0;
	struct hid_device_info *device_info = hid_enumerate(fcd_traits<ProPlus>::vendorId, fcd_traits<ProPlus>::productId);

	while (device_info != 0)
	{
		QString serialNumber = QString::fromWCharArray(device_info->serial_number);
		QString displayedName(QString("%1[%2] %3").arg(fcd_traits<ProPlus>::displayedName).arg(i).arg(serialNumber));

		result.append(SampleSourceDevice(displayedName,
				fcd_traits<ProPlus>::interfaceIID,
				serialNumber,
				i));

		device_info = device_info->next;
		i++;
	}

	return result;
}

PluginGUI* FCDProPlusPlugin::createSampleSourcePluginGUI(const QString& sourceId, const QString& sourceDisplayName, DeviceAPI *deviceAPI)
{
	if(sourceId == fcd_traits<ProPlus>::interfaceIID)
	{
		FCDProPlusGui* gui = new FCDProPlusGui(m_pluginAPI, deviceAPI);
		deviceAPI->setInputGUI(gui, sourceDisplayName);
		return gui;
	}
	else
	{
		return NULL;
	}
}
