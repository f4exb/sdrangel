#include <QtPlugin>
#include <QDebug>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "tcpiqplotgui.h"
#endif
#include "tcpiqplot.h"
#include "tcpiqplotplugin.h"

const PluginDescriptor TcpIqPlotPlugin::m_pluginDescriptor = {
    TcpIqPlot::m_channelId,
	QStringLiteral("TCP IQ"),
    QStringLiteral("7.22.9"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

TcpIqPlotPlugin::TcpIqPlotPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& TcpIqPlotPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void TcpIqPlotPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register TCP IQ Plot
	m_pluginAPI->registerRxChannel(TcpIqPlot::m_channelIdURI, TcpIqPlot::m_channelId, this);
}

void TcpIqPlotPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
	qDebug() << "TcpIqPlotPlugin::createRxChannel - Creating channel instance";
	
	if (bs || cs)
	{
		TcpIqPlot *instance = new TcpIqPlot(deviceAPI);
		qDebug() << "TcpIqPlotPlugin::createRxChannel - Instance created:" << (void*)instance;

		if (bs) {
			*bs = instance;
			qDebug() << "TcpIqPlotPlugin::createRxChannel - Set BasebandSampleSink";
		}

		if (cs) {
			*cs = instance;
			qDebug() << "TcpIqPlotPlugin::createRxChannel - Set ChannelAPI";
		}
	}
	
	qDebug() << "TcpIqPlotPlugin::createRxChannel - Complete";
}

#ifndef SERVER_MODE
ChannelGUI* TcpIqPlotPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
	(void) deviceUISet;
	(void) rxChannel;
	return TcpIqPlotGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* TcpIqPlotPlugin::createChannelWebAPIAdapter() const
{
	return nullptr; // No web API adapter for now
}