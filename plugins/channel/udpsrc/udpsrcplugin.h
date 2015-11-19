#ifndef INCLUDE_UDPSRCPLUGIN_H
#define INCLUDE_UDPSRCPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class UDPSrcPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.demod.udpsrc")

public:
	explicit UDPSrcPlugin(QObject* parent = 0);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceUDPSrc();
};

#endif // INCLUDE_UDPSRCPLUGIN_H
