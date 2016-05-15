#ifndef INCLUDE_AMPLUGIN_H
#define INCLUDE_AMPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class AMPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.channel.am")

public:
	explicit AMPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createChannel(const QString& channelName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceAM();
};

#endif // INCLUDE_AMPLUGIN_H
