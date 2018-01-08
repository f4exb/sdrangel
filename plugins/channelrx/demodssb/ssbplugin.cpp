#include "ssbplugin.h"

#include <device/devicesourceapi.h>
#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "ssbdemodgui.h"
#include "ssbdemod.h"

const PluginDescriptor SSBPlugin::m_pluginDescriptor = {
	QString("SSB Demodulator"),
	QString("3.10.1"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

SSBPlugin::SSBPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
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
	m_pluginAPI->registerRxChannel(SSBDemod::m_channelIdURI, SSBDemod::m_channelId, this);
}

PluginInstanceGUI* SSBPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	return SSBDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}

BasebandSampleSink* SSBPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new SSBDemod(deviceAPI);
}

ChannelSinkAPI* SSBPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new SSBDemod(deviceAPI);
}

