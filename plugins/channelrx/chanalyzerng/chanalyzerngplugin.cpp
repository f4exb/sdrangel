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

#include "plugin/pluginapi.h"
#include "chanalyzerngplugin.h"
#include "chanalyzernggui.h"
#include "chanalyzerng.h"

const PluginDescriptor ChannelAnalyzerNGPlugin::m_pluginDescriptor = {
	QString("Channel Analyzer NG"),
	QString("3.9.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

ChannelAnalyzerNGPlugin::ChannelAnalyzerNGPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& ChannelAnalyzerNGPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void ChannelAnalyzerNGPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerRxChannel(ChannelAnalyzerNG::m_channelIdURI, ChannelAnalyzerNG::m_channelId, this);
}

PluginInstanceGUI* ChannelAnalyzerNGPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    return ChannelAnalyzerNGGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}

BasebandSampleSink* ChannelAnalyzerNGPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new ChannelAnalyzerNG(deviceAPI);
}

ChannelSinkAPI* ChannelAnalyzerNGPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new ChannelAnalyzerNG(deviceAPI);
}

