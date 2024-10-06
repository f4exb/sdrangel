///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include "chanalyzer.h"
#include "chanalyzerplugin.h"
#include "chanalyzergui.h"
#include "chanalyzerwebapiadapter.h"

const PluginDescriptor ChannelAnalyzerPlugin::m_pluginDescriptor = {
    ChannelAnalyzer::m_channelId,
	QStringLiteral("Channel Analyzer"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

ChannelAnalyzerPlugin::ChannelAnalyzerPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& ChannelAnalyzerPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void ChannelAnalyzerPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerRxChannel(ChannelAnalyzer::m_channelIdURI, ChannelAnalyzer::m_channelId, this);
}

void ChannelAnalyzerPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		ChannelAnalyzer *instance = new ChannelAnalyzer(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

ChannelGUI* ChannelAnalyzerPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return ChannelAnalyzerGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}

ChannelWebAPIAdapter* ChannelAnalyzerPlugin::createChannelWebAPIAdapter() const
{
	return new ChannelAnalyzerWebAPIAdapter();
}
