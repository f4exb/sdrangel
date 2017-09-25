#include "chanalyzerplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "chanalyzergui.h"

const PluginDescriptor ChannelAnalyzerPlugin::m_pluginDescriptor = {
	QString("Channel Analyzer"),
	QString("2.0.0"),
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
	m_pluginAPI->registerRxChannel(ChannelAnalyzerGUI::m_channelID, this);
}

PluginInstanceGUI* ChannelAnalyzerPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == ChannelAnalyzerGUI::m_channelID)
	{
		ChannelAnalyzerGUI* gui = ChannelAnalyzerGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return NULL;
	}
}

void ChannelAnalyzerPlugin::createInstanceChannelAnalyzer(DeviceSourceAPI *deviceAPI)
{
	ChannelAnalyzerGUI::create(m_pluginAPI, deviceAPI);
}
