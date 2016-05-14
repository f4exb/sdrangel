#include <QtPlugin>
#include <QAction>
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
//	QAction* action = new QAction(tr("&LoRa Demodulator"), this);
//	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceLoRa()));
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.lora", this);
}

PluginGUI* LoRaPlugin::createChannel(const QString& channelName)
{
	if(channelName == "de.maintech.sdrangelove.channel.lora") {
		LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.lora", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void LoRaPlugin::createInstanceLoRa()
{
	LoRaDemodGUI* gui = LoRaDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.lora", gui);
	m_pluginAPI->addChannelRollup(gui);
}
