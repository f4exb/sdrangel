#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "ssbplugin.h"
#include "ssbdemodgui.h"

const PluginDescriptor SSBPlugin::m_pluginDescriptor = {
	QString("SSB Demodulator"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

SSBPlugin::SSBPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& SSBPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void SSBPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
//	QAction* action = new QAction(tr("&SSB Demodulator"), this);
//	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceSSB()));
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.ssb", this);
}

PluginGUI* SSBPlugin::createChannel(const QString& channelName)
{
	if(channelName == "de.maintech.sdrangelove.channel.ssb") {
		SSBDemodGUI* gui = SSBDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.ssb", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void SSBPlugin::createInstanceSSB()
{
	SSBDemodGUI* gui = SSBDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.ssb", gui);
	m_pluginAPI->addChannelRollup(gui);
}
