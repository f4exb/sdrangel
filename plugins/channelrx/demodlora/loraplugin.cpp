#include "../../channelrx/demodlora/loraplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "../../channelrx/demodlora/lorademodgui.h"

const PluginDescriptor LoRaPlugin::m_pluginDescriptor = {
	QString("LoRa Demodulator"),
	QString("2.0.0"),
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
	m_pluginAPI->registerRxChannel(LoRaDemodGUI::m_channelID, this);
}

PluginGUI* LoRaPlugin::createChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == LoRaDemodGUI::m_channelID)
	{
		LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return NULL;
	}
}

void LoRaPlugin::createInstanceLoRa(DeviceSourceAPI *deviceAPI)
{
	LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI, deviceAPI);
}
