#include "tcpsrcplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "tcpsrcgui.h"
#include "tcpsrc.h"

const PluginDescriptor TCPSrcPlugin::m_pluginDescriptor = {
	QString("TCP Channel Source"),
	QString("3.9.0"),
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
	m_pluginAPI->registerRxChannel(TCPSrc::m_channelIdURI, TCPSrc::m_channelId, this);
}

PluginInstanceGUI* TCPSrcPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	return TCPSrcGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}

BasebandSampleSink* TCPSrcPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new TCPSrc(deviceAPI);
}

ChannelSinkAPI* TCPSrcPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new TCPSrc(deviceAPI);
}

