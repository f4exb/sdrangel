#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "wfmdemodgui.h"
#include "wfmplugin.h"

const PluginDescriptor WFMPlugin::m_pluginDescriptor = {
	QString("WFM Demodulator"),
	QString("2.0.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

WFMPlugin::WFMPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& WFMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void WFMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register WFM demodulator
	m_pluginAPI->registerChannel(WFMDemodGUI::m_channelID, this);
}

PluginGUI* WFMPlugin::createChannel(const QString& channelName, DeviceAPI *deviceAPI)
{
	if(channelName == WFMDemodGUI::m_channelID)
	{
		WFMDemodGUI* gui = WFMDemodGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return NULL;
	}
}

void WFMPlugin::createInstanceWFM(DeviceAPI *deviceAPI)
{
	WFMDemodGUI* gui = WFMDemodGUI::create(m_pluginAPI, deviceAPI);
}
