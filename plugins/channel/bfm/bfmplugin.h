#ifndef INCLUDE_BFMPLUGIN_H
#define INCLUDE_BFMPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class BFMPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.channel.bfm")

public:
	explicit BFMPlugin(QObject* parent = 0);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceBFM();
};

#endif // INCLUDE_BFMPLUGIN_H
