///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "sdrdaemonsinkgui.h"
#include "sdrdaemonsinkplugin.h"

const PluginDescriptor SDRdaemonSinkPlugin::m_pluginDescriptor = {
	QString("SDRdaemon sink output"),
	QString("3.5.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString SDRdaemonSinkPlugin::m_hardwareID = "SDRdaemonSink";
const QString SDRdaemonSinkPlugin::m_deviceTypeID = SDRDAEMONSINK_DEVICE_TYPE_ID;

SDRdaemonSinkPlugin::SDRdaemonSinkPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& SDRdaemonSinkPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void SDRdaemonSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices SDRdaemonSinkPlugin::enumSampleSinks()
{
	SamplingDevices result;
	int count = 1;

	for(int i = 0; i < count; i++)
	{
		QString displayedName(QString("SDRdaemonSink[%1]").arg(i));

		result.append(SamplingDevice(displayedName,
		        m_hardwareID,
				m_deviceTypeID,
				QString::null,
				i));
	}

	return result;
}

PluginInstanceUI* SDRdaemonSinkPlugin::createSampleSinkPluginInstanceGUI(const QString& sinkId, QWidget **widget, DeviceSinkAPI *deviceAPI)
{
	if(sinkId == m_deviceTypeID)
	{
	    SDRdaemonSinkGui* gui = new SDRdaemonSinkGui(deviceAPI);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
