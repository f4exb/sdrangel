#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "amdemodgui.h"
#include "amdemod.h"
#include "amdemodplugin.h"

const PluginDescriptor AMDemodPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("3.9.0"),
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

PluginInstanceGUI* AMDemodPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	if(channelName == AMDemod::m_channelIdURI)
	{
		AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
		return gui;
	} else {
		return 0;
	}
}

BasebandSampleSink* AMDemodPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
    if(channelName == AMDemod::m_channelIdURI)
    {
        AMDemod* sink = new AMDemod(deviceAPI);
        return sink;
    } else {
        return 0;
    }
}

ChannelSinkAPI* AMDemodPlugin::createRxChannel(DeviceSourceAPI *deviceAPI)
{
    return new AMDemod(deviceAPI);
}

