#ifndef INCLUDE_NFMPLUGIN_H
#define INCLUDE_NFMPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceSourceAPI;

class NFMPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.channel.nfm")

public:
	explicit NFMPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginInstanceGUI* createRxChannel(const QString& channelName, DeviceSourceAPI *deviceAPI);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceNFM(DeviceSourceAPI *deviceAPI);
};

#endif // INCLUDE_NFMPLUGIN_H
