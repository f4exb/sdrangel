#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "ssbplugin.h"
#include "ssbdemodgui.h"

const PluginDescriptor SSBPlugin::m_pluginDescriptor = {
	QString("SSB Demodulator"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

SSBPlugin::SSBPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& SSBPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void SSBPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.ssb", this);
}

PluginGUI* SSBPlugin::createChannel(const QString& channelName, DeviceAPI *deviceAPI)
{
	if(channelName == "de.maintech.sdrangelove.channel.ssb") {
		SSBDemodGUI* gui = SSBDemodGUI::create(m_pluginAPI, deviceAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.ssb", gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void SSBPlugin::createInstanceSSB(DeviceAPI *deviceAPI)
{
	SSBDemodGUI* gui = SSBDemodGUI::create(m_pluginAPI, deviceAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.ssb", gui);
//	m_pluginAPI->addChannelRollup(gui);
}
