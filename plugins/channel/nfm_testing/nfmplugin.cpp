#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "nfmplugin.h"
#include "nfmdemodgui.h"

const PluginDescriptor NFMPlugin::m_pluginDescriptor = {
	QString("NFM Demodulator"),
	QString("---"),
	QString("(c) maintech GmbH (written by Christian Daniel)"),
	QString("http://www.maintech.de"),
	true,
	QString("http://www.maintech.de")
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
	QAction* action = new QAction(tr("&NFM Demodulator"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceNFM()));
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.nfm", this, action);
}

PluginGUI* NFMPlugin::createChannel(const QString& channelName)
{
	if(channelName == "de.maintech.sdrangelove.channel.nfm") {
		NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.nfm", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void NFMPlugin::createInstanceNFM()
{
	NFMDemodGUI* gui = NFMDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.nfm", gui);
	m_pluginAPI->addChannelRollup(gui);
}
