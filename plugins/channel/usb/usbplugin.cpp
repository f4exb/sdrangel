#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "usbplugin.h"
#include "usbdemodgui.h"

const PluginDescriptor USBPlugin::m_pluginDescriptor = {
	QString("USB Demodulator"),
	QString("0.1"),
	QString("(c) 2014 John Greb"),
	QString("http://www.maintech.de"),
	true,
	QString("github.com/hexameron/rtl-sdrangelove")
};

USBPlugin::USBPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& USBPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void USBPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	QAction* action = new QAction(tr("&USB Demodulator"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceUSB()));
	m_pluginAPI->registerChannel("de.maintech.sdrangelove.channel.usb", this, action);
}

PluginGUI* USBPlugin::createChannel(const QString& channelName)
{
	if(channelName == "de.maintech.sdrangelove.channel.usb") {
		USBDemodGUI* gui = USBDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.usb", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void USBPlugin::createInstanceUSB()
{
	USBDemodGUI* gui = USBDemodGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("de.maintech.sdrangelove.channel.usb", gui);
	m_pluginAPI->addChannelRollup(gui);
}
