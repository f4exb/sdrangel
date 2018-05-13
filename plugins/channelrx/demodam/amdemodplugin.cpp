#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "amdemodgui.h"
#endif
#include "amdemod.h"
#include "amdemodplugin.h"

const PluginDescriptor AMDemodPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("3.14.7"),
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
        DeviceUISet *deviceUISet __attribute__((unused)),
        BasebandSampleSink *rxChannel __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* AMDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	return AMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* AMDemodPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new AMDemod(deviceAPI);
}

ChannelSinkAPI* AMDemodPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new AMDemod(deviceAPI);
}

