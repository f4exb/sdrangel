#include "tcpsrcplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "tcpsrcgui.h"
#include "tcpsrc.h"

const PluginDescriptor TCPSrcPlugin::m_pluginDescriptor = {
	QString("TCP Channel Source"),
	QString("3.8.3"),
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
	m_pluginAPI->registerRxChannel(TCPSrc::m_channelID, this);
}

PluginInstanceGUI* TCPSrcPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == TCPSrc::m_channelID)
	{
		TCPSrcGUI* gui = TCPSrcGUI::create(m_pluginAPI, deviceUISet, rxChannel);
//		deviceAPI->registerChannelInstance("sdrangel.channel.tcpsrc", gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSink* TCPSrcPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == TCPSrc::m_channelID)
    {
        TCPSrc* sink = new TCPSrc(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}
