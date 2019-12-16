///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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
#include "freqtrackergui.h"
#endif
#include "freqtracker.h"
#include "freqtrackerwebapiadapter.h"
#include "freqtrackerplugin.h"

const PluginDescriptor FreqTrackerPlugin::m_pluginDescriptor = {
    FreqTracker::m_channelId,
	QString("Frequency Tracker"),
	QString("4.12.3"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

FreqTrackerPlugin::FreqTrackerPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& FreqTrackerPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FreqTrackerPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM demodulator
	m_pluginAPI->registerRxChannel(FreqTracker::m_channelIdURI, FreqTracker::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* FreqTrackerPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* FreqTrackerPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return FreqTrackerGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* FreqTrackerPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new FreqTracker(deviceAPI);
}

ChannelAPI* FreqTrackerPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new FreqTracker(deviceAPI);
}

ChannelWebAPIAdapter* FreqTrackerPlugin::createChannelWebAPIAdapter() const
{
	return new FreqTrackerWebAPIAdapter();
}
