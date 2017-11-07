#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "loraplugin.h"
#include "lorademodgui.h"

const PluginDescriptor LoRaPlugin::m_pluginDescriptor = {
	QString("LoRa Demodulator"),
	QString("3.8.2"),
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
	m_pluginAPI->registerRxChannel(LoRaDemodGUI::m_channelID, this);
}

PluginInstanceGUI* LoRaPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet)
{
	if(channelName == LoRaDemodGUI::m_channelID)
	{
		LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI, deviceUISet);
		return gui;
	} else {
		return NULL;
	}
}
