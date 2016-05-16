#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "loraplugin.h"
#include "lorademodgui.h"

const PluginDescriptor LoRaPlugin::m_pluginDescriptor = {
	QString("LoRa Demodulator"),
	QString("0.1"),
	QString("(c) 2015 John Greb"),
	QString("http://www.maintech.de"),
	true,
	QString("github.com/hexameron/rtl-sdrangelove")
};

LoRaPlugin::LoRaPlugin(QObject* parent) :
	QObject(parent)
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
	m_pluginAPI->registerChannel(LoRaDemodGUI::m_channelID, this);
}

PluginGUI* LoRaPlugin::createChannel(const QString& channelName, DeviceAPI *deviceAPI)
{
	if(channelName == LoRaDemodGUI::m_channelID)
	{
		LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI, deviceAPI);
//		deviceAPI->registerChannelInstance("de.maintech.sdrangelove.channel.lora", gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void LoRaPlugin::createInstanceLoRa(DeviceAPI *deviceAPI)
{
	LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI, deviceAPI);
//	deviceAPI->registerChannelInstance("de.maintech.sdrangelove.channel.lora", gui);
//	m_pluginAPI->addChannelRollup(gui);
}
