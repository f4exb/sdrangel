#include "../../channelrx/demodnfm/nfmplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "../../channelrx/demodnfm/nfmdemodgui.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
	QString("NFM Demodulator"),
	QString("3.1.1"),
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
	m_pluginAPI->registerRxChannel(NFMDemodGUI::m_channelID, this);
}

PluginGUI* NFMPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == NFMDemodGUI::m_channelID) {
		NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return NULL;
	}
}

void NFMPlugin::createInstanceNFM(DeviceSourceAPI *deviceAPI)
{
	NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI, deviceAPI);
}
