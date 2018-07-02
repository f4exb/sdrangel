#include "ssbplugin.h"

#include <device/devicesourceapi.h>
#include <QtPlugin>
#include "plugin/pluginapi.h"
#ifndef SERVER_MODE
#include "ssbdemodgui.h"
#endif
#include "ssbdemod.h"

const PluginDescriptor SSBPlugin::m_pluginDescriptor = {
	QString("SSB Demodulator"),
	QString("4.0.2"),
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

#ifdef SERVER_MODE
PluginInstanceGUI* SSBPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet __attribute__((unused)),
        BasebandSampleSink *rxChannel __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* SSBPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	return SSBDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* SSBPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new SSBDemod(deviceAPI);
}

ChannelSinkAPI* SSBPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new SSBDemod(deviceAPI);
}

