#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "nfmplugin.h"
#include "nfmdemodgui.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
	QString("NFM Demodulator"),
	QString("3.7.9"),
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
	m_pluginAPI->registerRxChannel(NFMDemodGUI::m_channelID, this);
}

PluginInstanceGUI* NFMPlugin::createRxChannel(const QString& channelName, DeviceUISet *deviceUISet)
{
	if(channelName == NFMDemodGUI::m_channelID) {
		NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI, deviceUISet);
		return gui;
	} else {
		return NULL;
	}
}

void NFMPlugin::createInstanceNFM(DeviceUISet *deviceUISet)
{
	NFMDemodGUI::create(m_pluginAPI, deviceUISet);
}
