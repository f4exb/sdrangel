#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "amdemodgui.h"
#endif
#include "amdemod.h"
#include "amdemodwebapiadapter.h"
#include "amdemodplugin.h"

const PluginDescriptor AMDemodPlugin::m_pluginDescriptor = {
    AMDemod::m_channelId,
	QString("AM Demodulator"),
	QString("4.12.3"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

AMDemodPlugin::AMDemodPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& AMDemodPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AMDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM demodulator
	m_pluginAPI->registerRxChannel(AMDemod::m_channelIdURI, AMDemod::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* AMDemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* AMDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return AMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* AMDemodPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new AMDemod(deviceAPI);
}

ChannelAPI* AMDemodPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new AMDemod(deviceAPI);
}

ChannelWebAPIAdapter* AMDemodPlugin::createChannelWebAPIAdapter() const
{
	return new AMDemodWebAPIAdapter();
}