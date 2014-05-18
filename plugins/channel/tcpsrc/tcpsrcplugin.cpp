#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "tcpsrcplugin.h"
#include "tcpsrcgui.h"

const PluginDescriptor TCPSrcPlugin::m_pluginDescriptor = {
	QString("TCP Channel Source"),
	QString("---"),
	QString("(c) maintech GmbH (written by Christian Daniel)"),
	QString("http://www.maintech.de"),
	true,
	QString("http://www.maintech.de")
};

TCPSrcPlugin::TCPSrcPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& TCPSrcPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void TCPSrcPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register TCP Channel Source
	QAction* action = new QAction(tr("&TCP Source"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceTCPSrc()));
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.tcpsrc", this, action);
}

PluginGUI* TCPSrcPlugin::createChannel(const QString& channelName)
{
	if(channelName == "de.maintech.sdrangelove.channel.tcpsrc") {
		TCPSrcGUI* gui = TCPSrcGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.tcpsrc", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void TCPSrcPlugin::createInstanceTCPSrc()
{
	TCPSrcGUI* gui = TCPSrcGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.tcpsrc", gui);
	m_pluginAPI->addChannelRollup(gui);
}
