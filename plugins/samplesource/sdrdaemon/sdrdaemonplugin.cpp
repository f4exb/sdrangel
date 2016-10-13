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

#include "sdrdaemongui.h"
#include "sdrdaemonplugin.h"
#include <device/devicesourceapi.h>

const PluginDescriptor SDRdaemonPlugin::m_pluginDescriptor = {
	QString("SDRdaemon input"),
	QString("2.0.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString SDRdaemonPlugin::m_deviceTypeID = SDRDAEMON_DEVICE_TYPE_ID;

SDRdaemonPlugin::SDRdaemonPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& SDRdaemonPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void SDRdaemonPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices SDRdaemonPlugin::enumSampleSources()
{
	SamplingDevices result;
	int count = 1;

	for(int i = 0; i < count; i++)
	{
		QString displayedName(QString("SDRdaemon[%1]").arg(i));

		result.append(SamplingDevice(displayedName,
				m_deviceTypeID,
				QString::null,
				i));
	}

	return result;
}

PluginGUI* SDRdaemonPlugin::createSampleSourcePluginGUI(const QString& sourceId, QWidget **widget, DeviceSourceAPI *deviceAPI)
{
	if(sourceId == m_deviceTypeID)
	{
		SDRdaemonGui* gui = new SDRdaemonGui(deviceAPI);
		*widget = gui;
		return gui;
	}
	else
	{
		return NULL;
	}
}
