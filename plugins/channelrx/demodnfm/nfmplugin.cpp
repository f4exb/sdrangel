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
	QString("4.12.3"),
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

#ifdef SERVER_MODE
PluginInstanceGUI* NFMPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* NFMPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return NFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* NFMPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new NFMDemod(deviceAPI);
}

ChannelAPI* NFMPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new NFMDemod(deviceAPI);
}

ChannelWebAPIAdapter* NFMPlugin::createChannelWebAPIAdapter() const
{
	return new NFMDemodWebAPIAdapter();
}
