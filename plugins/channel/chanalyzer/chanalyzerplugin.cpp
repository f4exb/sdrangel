#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "chanalyzergui.h"
#include "chanalyzerplugin.h"

const PluginDescriptor ChannelAnalyzerPlugin::m_pluginDescriptor = {
	QString("Channel Analyzer"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

ChannelAnalyzerPlugin::ChannelAnalyzerPlugin(QObject* parent) :
	QObject(parent)
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
	m_pluginAPI->registerChannel("org.f4exb.sdrangelove.channel.chanalyzer", this);
}

PluginGUI* ChannelAnalyzerPlugin::createChannel(const QString& channelName, DeviceAPI *deviceAPI)
{
	if(channelName == "org.f4exb.sdrangelove.channel.chanalyzer") {
		ChannelAnalyzerGUI* gui = ChannelAnalyzerGUI::create(m_pluginAPI, deviceAPI);
		m_pluginAPI->registerChannelInstance("org.f4exb.sdrangelove.channel.chanalyzer", gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void ChannelAnalyzerPlugin::createInstanceChannelAnalyzer(DeviceAPI *deviceAPI)
{
	ChannelAnalyzerGUI* gui = ChannelAnalyzerGUI::create(m_pluginAPI, deviceAPI);
	m_pluginAPI->registerChannelInstance("org.f4exb.sdrangelove.channel.chanalyzer", gui);
//	m_pluginAPI->addChannelRollup(gui);
}
