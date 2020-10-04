#include "ssbplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"
#ifndef SERVER_MODE
#include "ssbdemodgui.h"
#endif
#include "ssbdemod.h"
#include "ssbdemodwebapiadapter.h"
#include "ssbplugin.h"

const PluginDescriptor SSBPlugin::m_pluginDescriptor = {
    SSBDemod::m_channelId,
	QString("SSB Demodulator"),
	QString("4.12.3"),
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

void SSBPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		SSBDemod *instance = new SSBDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* SSBPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
ChannelGUI* SSBPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return SSBDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* SSBPlugin::createChannelWebAPIAdapter() const
{
	return new SSBDemodWebAPIAdapter();
}
