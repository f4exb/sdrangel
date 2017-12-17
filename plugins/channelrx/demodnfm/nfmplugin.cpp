#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "nfmplugin.h"
#include "nfmdemodgui.h"
#include "nfmdemod.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
	QString("NFM Demodulator"),
	QString("3.9.0"),
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
        const QString& channelName __attribute__((unused)),
        DeviceUISet *deviceUISet __attribute__((unused)),
        BasebandSampleSink *rxChannel __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* NFMPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == NFMDemod::m_channelIdURI) {
		NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
		return gui;
	} else {
		return 0;
	}
}
#endif

BasebandSampleSink* NFMPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == NFMDemod::m_channelIdURI)
    {
        NFMDemod* sink = new NFMDemod(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}

void NFMPlugin::createRxChannel(ChannelSinkAPI **channelSinkAPI, const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == NFMDemod::m_channelIdURI) {
        *channelSinkAPI = new NFMDemod(deviceAPI);
    } else {
        *channelSinkAPI = 0;
    }
}

