#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "chanalyzergui.h"
#include "chanalyzerplugin.h"

const PluginDescriptor ChannelAnalyzerPlugin::m_pluginDescriptor = {
	QString("Channel Analyzer"),
	QString("1.0"),
	QString("(c) 2015 Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/rtl-sdrangelove/tree/f4exb"),
	true,
	QString("https://github.com/f4exb/rtl-sdrangelove/tree/f4exb")
};

ChannelAnalyzerPlugin::ChannelAnalyzerPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& ChannelAnalyzerPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void ChannelAnalyzerPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register demodulator
	QAction* action = new QAction(tr("&ChannelAnalyzer"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceChannelAnalyzer()));
	m_pluginAPI->registerChannel("org.f4exb.sdrangelove.channel.chanalyzer", this, action);
}

PluginGUI* ChannelAnalyzerPlugin::createChannel(const QString& channelName)
{
	if(channelName == "org.f4exb.sdrangelove.channel.chanalyzer") {
		ChannelAnalyzerGUI* gui = ChannelAnalyzerGUI::create(m_pluginAPI);
		m_pluginAPI->registerChannelInstance("org.f4exb.sdrangelove.channel.chanalyzer", gui);
		m_pluginAPI->addChannelRollup(gui);
		return gui;
	} else {
		return NULL;
	}
}

void ChannelAnalyzerPlugin::createInstanceChannelAnalyzer()
{
	ChannelAnalyzerGUI* gui = ChannelAnalyzerGUI::create(m_pluginAPI);
	m_pluginAPI->registerChannelInstance("org.f4exb.sdrangelove.channel.chanalyzer", gui);
	m_pluginAPI->addChannelRollup(gui);
}
