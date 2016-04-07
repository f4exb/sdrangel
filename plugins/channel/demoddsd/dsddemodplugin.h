#ifndef INCLUDE_DSDDEMODLUGIN_H
#define INCLUDE_DSDDEMODLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DSDDemodPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.channel.dsddemod")

public:
	explicit DSDDemodPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceDSDDemod();
};

#endif // INCLUDE_DSDDEMODLUGIN_H
