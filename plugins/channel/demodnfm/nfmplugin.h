#ifndef INCLUDE_NFMPLUGIN_H
#define INCLUDE_NFMPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceAPI;

class NFMPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.channel.nfm")

public:
	explicit NFMPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName, DeviceAPI *deviceAPI);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceNFM(DeviceAPI *deviceAPI);
};

#endif // INCLUDE_NFMPLUGIN_H
