#include "../../channelrx/demodam/amplugin.h"

#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "../../channelrx/demodam/amdemodgui.h"

const PluginDescriptor AMPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("2.0.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

AMPlugin::AMPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM demodulator
	m_pluginAPI->registerChannel(AMDemodGUI::m_channelID, this);
}

PluginGUI* AMPlugin::createChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == AMDemodGUI::m_channelID)
	{
		AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return NULL;
	}
}

void AMPlugin::createInstanceAM(DeviceSourceAPI *deviceAPI)
{
	AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceAPI);
}
