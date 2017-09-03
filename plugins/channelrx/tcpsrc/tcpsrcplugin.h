#ifndef INCLUDE_TCPSRCPLUGIN_H
#define INCLUDE_TCPSRCPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceSourceAPI;

class TCPSrcPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.demod.tcpsrc")

public:
	explicit TCPSrcPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginInstanceUI* createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceTCPSrc(DeviceSourceAPI *deviceAPI);
};

#endif // INCLUDE_TCPSRCPLUGIN_H
