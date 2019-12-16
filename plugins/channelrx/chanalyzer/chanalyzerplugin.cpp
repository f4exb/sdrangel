///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
	QString("Channel Analyzer"),
	QString("4.12.3"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
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

PluginInstanceGUI* ChannelAnalyzerPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return ChannelAnalyzerGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}

BasebandSampleSink* ChannelAnalyzerPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new ChannelAnalyzer(deviceAPI);
}

ChannelAPI* ChannelAnalyzerPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new ChannelAnalyzer(deviceAPI);
}

ChannelWebAPIAdapter* ChannelAnalyzerPlugin::createChannelWebAPIAdapter() const
{
	return new ChannelAnalyzerWebAPIAdapter();
}