#ifndef INCLUDE_TCPSRCPLUGIN_H
#define INCLUDE_TCPSRCPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class TCPSrcPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.demod.tcpsrc")

public:
	explicit TCPSrcPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceTCPSrc();
};

#endif // INCLUDE_TCPSRCPLUGIN_H
