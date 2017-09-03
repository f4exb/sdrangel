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
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

#include "device/devicesinkapi.h"
#include "filesinkgui.h"
#include "filesinkplugin.h"

const PluginDescriptor FileSinkPlugin::m_pluginDescriptor = {
	QString("File sink output"),
	QString("3.5.0"),
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
	int count = 1;

	for(int i = 0; i < count; i++)
	{
		QString displayedName(QString("FileSink[%1]").arg(i));

		result.append(SamplingDevice(displayedName,
		        m_hardwareID,
				m_deviceTypeID,
				QString::null,
				i));
	}

	return result;
}

PluginInstanceUI* FileSinkPlugin::createSampleSinkPluginInstanceUI(const QString& sinkId, QWidget **widget, DeviceSinkAPI *deviceAPI)
{
	if(sinkId == m_deviceTypeID)
	{
		FileSinkGui* gui = new FileSinkGui(deviceAPI);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
