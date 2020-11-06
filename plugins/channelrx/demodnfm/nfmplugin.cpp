#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "nfmplugin.h"
#ifndef SERVER_MODE
#include "nfmdemodgui.h"
#endif
#include "nfmdemod.h"
#include "nfmdemodwebapiadapter.h"
#include "nfmplugin.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
    NFMDemod::m_channelId,
	QString("NFM Demodulator"),
	QString("4.21.2"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

NFMPlugin::NFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& NFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void NFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register NFM demodulator
	m_pluginAPI->registerRxChannel(NFMDemod::m_channelIdURI, NFMDemod::m_channelId, this);
}

void NFMPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	if (bs || cs)
	{
		NFMDemod *instance = new NFMDemod(deviceAPI);

		if (bs) {
			*bs = instance;
		}

		if (cs) {
			*cs = instance;
		}
	}
}

#ifdef SERVER_MODE
ChannelGUI* NFMPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
ChannelGUI* NFMPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return NFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* NFMPlugin::createChannelWebAPIAdapter() const
{
	return new NFMDemodWebAPIAdapter();
}
