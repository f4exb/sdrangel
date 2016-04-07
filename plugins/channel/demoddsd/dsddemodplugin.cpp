#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "dsddemodplugin.h"
#include "dsddemodgui.h"

const PluginDescriptor DSDDemodPlugin::m_pluginDescriptor = {
	QString("DSD Demodulator"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

DSDDemodPlugin::DSDDemodPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& DSDDemodPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void DSDDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register DSD demodulator
	QAction* action = new QAction(tr("&DSD Demodulator"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceDSDDemod()));
	m_pluginAPI->registerChannel("sdrangel.channel.dsddemod", this, action);
}

PluginGUI* DSDDemodPlugin::createChannel(const QString& channelName)
{
	if(channelName == "sdrangel.channel.dsddemod") {
		DSDDemodGUI* gui = DSDDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("sdrangel.channel.dsddemod", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void DSDDemodPlugin::createInstanceDSDDemod()
{
    DSDDemodGUI* gui = DSDDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("sdrangel.channel.dsddemod", gui);
	m_pluginAPI->addChannelRollup(gui);
}
