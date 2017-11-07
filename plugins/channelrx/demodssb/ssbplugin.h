#ifndef INCLUDE_SSBPLUGIN_H
#define INCLUDE_SSBPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;

class SSBPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.channel.ssb")

public:
	explicit SSBPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginInstanceGUI* createRxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceSSB(DeviceUISet *deviceUISet);
};

#endif // INCLUDE_SSBPLUGIN_H
