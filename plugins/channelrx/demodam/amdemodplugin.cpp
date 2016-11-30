#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "amdemodgui.h"
#include "amdemodplugin.h"

const PluginDescriptor AMDemodPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("2.4.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

AMDemodPlugin::AMDemodPlugin(QObject* parent) :
	QObject(parent)
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

PluginGUI* AMDemodPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
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
	AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI, deviceAPI);
}
