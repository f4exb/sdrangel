#include "wfmplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "wfmdemodgui.h"
#endif
#include "wfmdemod.h"
#include "wfmdemodwebapiadapter.h"
#include "wfmplugin.h"

const PluginDescriptor WFMPlugin::m_pluginDescriptor = {
    WFMDemod::m_channelId,
	QString("WFM Demodulator"),
	QString("4.12.3"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

WFMPlugin::WFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& WFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void WFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register WFM demodulator
	m_pluginAPI->registerRxChannel(WFMDemod::m_channelIdURI, WFMDemod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* WFMPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* WFMPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return WFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* WFMPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new WFMDemod(deviceAPI);
}

ChannelAPI* WFMPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new WFMDemod(deviceAPI);
}

ChannelWebAPIAdapter* WFMPlugin::createChannelWebAPIAdapter() const
{
	return new WFMDemodWebAPIAdapter();
}
