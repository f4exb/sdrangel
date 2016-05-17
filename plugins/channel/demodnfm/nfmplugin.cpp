#include <QtPlugin>
#include "plugin/pluginapi.h"
#include "nfmplugin.h"
#include "nfmdemodgui.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
	QString("NFM Demodulator"),
	QString("2.0.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

NFMPlugin::NFMPlugin(QObject* parent) :
	QObject(parent)
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
	m_pluginAPI->registerChannel(NFMDemodGUI::m_channelID, this);
}

PluginGUI* NFMPlugin::createChannel(const QString& channelName, DeviceAPI *deviceAPI)
{
	if(channelName == NFMDemodGUI::m_channelID) {
		NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI, deviceAPI);
//		deviceAPI->registerChannelInstance(NFMDemodGUI::m_channelID, gui);
//		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void NFMPlugin::createInstanceNFM(DeviceAPI *deviceAPI)
{
	NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI, deviceAPI);
//	deviceAPI->registerChannelInstance(NFMDemodGUI::m_channelID, gui);
//	m_pluginAPI->addChannelRollup(gui);
}
