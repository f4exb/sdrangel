#include "chanalyzerplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "chanalyzergui.h"
#include "chanalyzer.h"

const PluginDescriptor ChannelAnalyzerPlugin::m_pluginDescriptor = {
	QString("Channel Analyzer"),
	QString("3.9.0"),
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

PluginInstanceGUI* ChannelAnalyzerPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    return ChannelAnalyzerGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}

BasebandSampleSink* ChannelAnalyzerPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new ChannelAnalyzer(deviceAPI);
}

ChannelSinkAPI* ChannelAnalyzerPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new ChannelAnalyzer(deviceAPI);
}
