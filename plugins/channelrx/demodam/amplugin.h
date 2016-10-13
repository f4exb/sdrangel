#ifndef INCLUDE_AMPLUGIN_H
#define INCLUDE_AMPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceSourceAPI;

class AMPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.channel.am")

public:
	explicit AMPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceAM(DeviceSourceAPI *deviceAPI);
};

#endif // INCLUDE_AMPLUGIN_H
