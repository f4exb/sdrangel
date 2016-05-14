#include "amplugin.h"

#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "amdemodgui.h"

const PluginDescriptor AMPlugin::m_pluginDescriptor = {
	QString("AM Demodulator"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

AMPlugin::AMPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AMPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AMPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AM demodulator
	QAction* action = new QAction(tr("&AM Demodulator"), this);
//	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceAM()));
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.am", this, action);
}

PluginGUI* AMPlugin::createChannel(const QString& channelName)
{
	if(channelName == "de.maintech.sdrangelove.channel.am") {
	    return createInstanceAM();
	} else {
		return NULL;
	}
}

PluginGUI*  AMPlugin::createInstanceAM()
{
	AMDemodGUI* gui = AMDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.am", gui);
	m_pluginAPI->addChannelRollup(gui);
	return gui;
}
