#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "amdemodgui.h"
#include "amdemodplugin.h"

const PluginDescriptor AMDemodPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("3.8.2"),
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
	m_pluginAPI->registerRxChannel(AMDemodGUI::m_channelID, this);
}

PluginInstanceGUI* AMDemodPlugin::createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet)
{
	if(channelName == AMDemodGUI::m_channelID)
	{
		AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceUISet);
		return gui;
	} else {
		return NULL;
	}
}
