#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"

#include "wfmdemodgui.h"
#include "wfmplugin.h"

const PluginDescriptor WFMPlugin::m_pluginDescriptor = {
	QString("WFM Demodulator"),
	QString("---"),
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
//	QAction* action = new QAction(tr("&WFM Demodulator"), this);
//	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceWFM()));
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.wfm", this);
}

PluginGUI* WFMPlugin::createChannel(const QString& channelName)
{
	if(channelName == "de.maintech.sdrangelove.channel.wfm") {
		WFMDemodGUI* gui = WFMDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.wfm", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void WFMPlugin::createInstanceWFM()
{
	WFMDemodGUI* gui = WFMDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.wfm", gui);
	m_pluginAPI->addChannelRollup(gui);
}
