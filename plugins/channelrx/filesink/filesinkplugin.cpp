///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef SERVER_MODE
#include "filesinkgui.h"
#endif
#include "filesink.h"
#include "filesinkwebapiadapter.h"
#include "filesinkplugin.h"

const PluginDescriptor FileSinkPlugin::m_pluginDescriptor = {
    FileSink::m_channelId,
    QStringLiteral("File Sink"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

FileSinkPlugin::FileSinkPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& FileSinkPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void FileSinkPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel Source
    m_pluginAPI->registerRxChannel(FileSink::m_channelIdURI, FileSink::m_channelId, this);
}

void FileSinkPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		FileSink *instance = new FileSink(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* FileSinkPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* FileSinkPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return FileSinkGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* FileSinkPlugin::createChannelWebAPIAdapter() const
{
	return new FileSinkWebAPIAdapter();
}
