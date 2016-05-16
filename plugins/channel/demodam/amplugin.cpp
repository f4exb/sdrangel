#include "amplugin.h"

#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "amdemodgui.h"

const PluginDescriptor AMPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

AMPlugin::AMPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM demodulator
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.am", this);
}

PluginGUI* AMPlugin::createChannel(const QString& channelName, DeviceAPI *deviceAPI)
{
	if(channelName == "de.maintech.sdrangelove.channel.am") {
		AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.am", gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void AMPlugin::createInstanceAM(DeviceAPI *deviceAPI)
{
	AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.am", gui);
//	m_pluginAPI->addChannelRollup(gui);
}
