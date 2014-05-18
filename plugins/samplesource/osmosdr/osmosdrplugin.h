#ifndef INCLUDE_OSMOSDRPLUGIN_H
#define INCLUDE_OSMOSDRPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class OsmoSDRPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "org.osmocom.sdr.samplesource.osmo-sdr")

public:
	explicit OsmoSDRPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	SampleSourceDevices enumSampleSources();
	PluginGUI* createSampleSource(const QString& sourceName, const QByteArray& address);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_OSMOSDRPLUGIN_H
