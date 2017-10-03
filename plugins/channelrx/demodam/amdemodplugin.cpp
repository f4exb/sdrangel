#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "amdemodgui.h"
#include "amdemodplugin.h"

const PluginDescriptor AMDemodPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("3.7.3"),
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

PluginInstanceGUI* AMDemodPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == AMDemodGUI::m_channelID)
	{
		AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return NULL;
	}
}

void AMDemodPlugin::createInstanceDemodAM(DeviceSourceAPI *deviceAPI)
{
	AMDemodGUI::create(m_pluginAPI, deviceAPI);
}
