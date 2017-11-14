#include "chanalyzerplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "chanalyzergui.h"
#include "chanalyzer.h"

const PluginDescriptor ChannelAnalyzerPlugin::m_pluginDescriptor = {
	QString("Channel Analyzer"),
	QString("3.8.4"),
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
	m_pluginAPI->registerRxChannel(ChannelAnalyzer::m_channelID, this);
}

PluginInstanceGUI* ChannelAnalyzerPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == ChannelAnalyzer::m_channelID)
	{
		ChannelAnalyzerGUI* gui = ChannelAnalyzerGUI::create(m_pluginAPI, deviceUISet, rxChannel);
		return gui;
	} else {
		return NULL;
	}
}


BasebandSampleSink* ChannelAnalyzerPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == ChannelAnalyzer::m_channelID)
    {
        ChannelAnalyzer* sink = new ChannelAnalyzer(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}
