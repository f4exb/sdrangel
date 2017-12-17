#include "wfmplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "wfmdemodgui.h"
#include "wfmdemod.h"

const PluginDescriptor WFMPlugin::m_pluginDescriptor = {
	QString("WFM Demodulator"),
	QString("3.8.5"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

WFMPlugin::WFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& WFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void WFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register WFM demodulator
	m_pluginAPI->registerRxChannel(WFMDemod::m_channelIdURI, WFMDemod::m_channelId, this);
}

PluginInstanceGUI* WFMPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == WFMDemod::m_channelIdURI)
	{
		WFMDemodGUI* gui = WFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSink* WFMPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == WFMDemod::m_channelIdURI)
    {
        WFMDemod* sink = new WFMDemod(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}

void WFMPlugin::createRxChannel(ChannelSinkAPI **channelSinkAPI, const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == WFMDemod::m_channelIdURI) {
        *channelSinkAPI = new WFMDemod(deviceAPI);
    } else {
        *channelSinkAPI = 0;
    }
}

