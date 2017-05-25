#include "wfmplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#include "wfmdemodgui.h"

const PluginDescriptor WFMPlugin::m_pluginDescriptor = {
	QString("WFM Demodulator"),
	QString("3.5.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

WFMPlugin::WFMPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(0)
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
	m_pluginAPI->registerRxChannel(WFMDemodGUI::m_channelID, this);
}

PluginGUI* WFMPlugin::createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI)
{
	if(channelName == WFMDemodGUI::m_channelID)
	{
		WFMDemodGUI* gui = WFMDemodGUI::create(m_pluginAPI, deviceAPI);
		return gui;
	} else {
		return NULL;
	}
}

void WFMPlugin::createInstanceWFM(DeviceSourceAPI *deviceAPI)
{
	WFMDemodGUI::create(m_pluginAPI, deviceAPI);
}
