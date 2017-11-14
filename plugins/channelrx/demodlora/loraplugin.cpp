#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "loraplugin.h"
#include "lorademodgui.h"
#include "lorademod.h"

const PluginDescriptor LoRaPlugin::m_pluginDescriptor = {
	QString("LoRa Demodulator"),
	QString("3.8.4"),
	QString("(c) 2015 John Greb"),
	QString("http://www.maintech.de"),
	true,
	QString("github.com/hexameron/rtl-sdrangelove")
};

LoRaPlugin::LoRaPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& LoRaPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void LoRaPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerRxChannel(LoRaDemod::m_channelID, this);
}

PluginInstanceGUI* LoRaPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == LoRaDemod::m_channelID)
	{
		LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSink* LoRaPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == LoRaDemod::m_channelID)
    {
        LoRaDemod* sink = new LoRaDemod(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}
