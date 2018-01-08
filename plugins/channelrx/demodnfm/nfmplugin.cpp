#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "nfmplugin.h"
#include "nfmdemodgui.h"
#include "nfmdemod.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
	QString("NFM Demodulator"),
	QString("3.10.1"),
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
        DeviceUISet *deviceUISet __attribute__((unused)),
        BasebandSampleSink *rxChannel __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* NFMPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	return NFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* NFMPlugin::createRxChannelBS(DeviceSourceAPI *deviceAPI)
{
    return new NFMDemod(deviceAPI);
}

ChannelSinkAPI* NFMPlugin::createRxChannelCS(DeviceSourceAPI *deviceAPI)
{
    return new NFMDemod(deviceAPI);
}

