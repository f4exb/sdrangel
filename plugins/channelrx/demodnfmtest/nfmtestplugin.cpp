#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "nfmtestdemodgui.h"
#endif
#include "nfmtestdemod.h"
#include "nfmtestdemodwebapiadapter.h"
#include "nfmtestplugin.h"

const PluginDescriptor NFMTestPlugin::m_pluginDescriptor = {
	QString("NFM Test Demodulator"),
	QString("4.12.1"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

NFMTestPlugin::NFMTestPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
{
}

const PluginDescriptor& NFMTestPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void NFMTestPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register NFM demodulator
	m_pluginAPI->registerRxChannel(NFMDemodTest::m_channelIdURI, NFMDemodTest::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* NFMTestPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* NFMTestPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	return NFMDemodTestGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

BasebandSampleSink* NFMTestPlugin::createRxChannelBS(DeviceAPI *deviceAPI) const
{
    return new NFMDemodTest(deviceAPI);
}

ChannelAPI* NFMTestPlugin::createRxChannelCS(DeviceAPI *deviceAPI) const
{
    return new NFMDemodTest(deviceAPI);
}

ChannelWebAPIAdapter* NFMTestPlugin::createChannelWebAPIAdapter() const
{
	return new NFMDemodTestWebAPIAdapter();
}
