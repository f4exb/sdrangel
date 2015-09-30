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

#include "filesourcegui.h"
#include "filesourceplugin.h"

const PluginDescriptor FileSourcePlugin::m_pluginDescriptor = {
	QString("File source input"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

FileSourcePlugin::FileSourcePlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FileSourcePlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FileSourcePlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;
	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.filesource", this);
}

PluginInterface::SampleSourceDevices FileSourcePlugin::enumSampleSources()
{
	SampleSourceDevices result;
	int count = 1;

	for(int i = 0; i < count; i++)
	{
		QString displayedName(QString("FileSource #%1 choose file").arg(i));
		SimpleSerializer s(1);
		s.writeS32(1, i);
		s.writeString(2, "");

		result.append(SampleSourceDevice(displayedName,
				"org.osmocom.sdr.samplesource.filesource",
				QString::null,
				i));
	}

	return result;
}

PluginGUI* FileSourcePlugin::createSampleSourcePluginGUI(const QString& sourceId)
{
	if(sourceId == "org.osmocom.sdr.samplesource.filesource") {
		FileSourceGui* gui = new FileSourceGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	} else {
		return NULL;
	}
}
