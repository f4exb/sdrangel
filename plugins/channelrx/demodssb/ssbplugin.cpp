#include "ssbplugin.h"

#include <device/devicesourceapi.h>
#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "ssbdemodgui.h"
#include "ssbdemod.h"

const PluginDescriptor SSBPlugin::m_pluginDescriptor = {
	QString("SSB Demodulator"),
	QString("3.8.4"),
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
	m_pluginAPI->registerRxChannel(SSBDemod::m_channelID, this);
}

PluginInstanceGUI* SSBPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == SSBDemod::m_channelID)
	{
		SSBDemodGUI* gui = SSBDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSink* SSBPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == SSBDemod::m_channelID)
    {
        SSBDemod* sink = new SSBDemod(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}
