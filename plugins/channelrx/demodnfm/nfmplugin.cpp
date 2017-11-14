#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "nfmplugin.h"
#include "nfmdemodgui.h"
#include "nfmdemod.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
	QString("NFM Demodulator"),
	QString("3.8.4"),
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
	m_pluginAPI->registerRxChannel(NFMDemod::m_channelID, this);
}

PluginInstanceGUI* NFMPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == NFMDemod::m_channelID) {
		NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSink* NFMPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == NFMDemod::m_channelID)
    {
        NFMDemod* sink = new NFMDemod(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}
