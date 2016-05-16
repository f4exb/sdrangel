#ifndef INCLUDE_LoRaPLUGIN_H
#define INCLUDE_LoRaPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceAPI;

class LoRaPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.channel.lora")

public:
	explicit LoRaPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName, DeviceAPI *deviceAPI);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceLoRa(DeviceAPI *deviceAPI);
};

#endif // INCLUDE_LoRaPLUGIN_H
