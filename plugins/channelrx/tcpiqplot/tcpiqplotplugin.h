#ifndef INCLUDE_TCPIQPLOTPLUGIN_H
#define INCLUDE_TCPIQPLOTPLUGIN_H

#include <QtPlugin>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSink;

class TcpIqPlotPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.channel.tcpiqplot")

public:
	explicit TcpIqPlotPlugin(QObject* parent = nullptr);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual void createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const;
	virtual ChannelGUI* createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const;
	virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_TCPIQPLOTPLUGIN_H