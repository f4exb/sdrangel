#include "../../channelrx/tcpsrc/tcpsrcplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "../../channelrx/tcpsrc/tcpsrcgui.h"

const PluginDescriptor TCPSrcPlugin::m_pluginDescriptor = {
	QString("TCP Channel Source"),
	QString("3.5.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

TCPSrcPlugin::TCPSrcPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& TCPSrcPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void TCPSrcPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register TCP Channel Source
	m_pluginAPI->registerRxChannel(TCPSrcGUI::m_channelID, this);
}

PluginInstanceGUI* TCPSrcPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == TCPSrcGUI::m_channelID)
	{
		TCPSrcGUI* gui = TCPSrcGUI::create(m_pluginAPI, deviceAPI);
//		deviceAPI->registerChannelInstance("sdrangel.channel.tcpsrc", gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void TCPSrcPlugin::createInstanceTCPSrc(DeviceSourceAPI *deviceAPI)
{
	TCPSrcGUI::create(m_pluginAPI, deviceAPI);
//	deviceAPI->registerChannelInstance("sdrangel.channel.tcpsrc", gui);
//	m_pluginAPI->addChannelRollup(gui);
}
