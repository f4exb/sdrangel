#ifndef INCLUDE_FCDPLUGIN_H
#define INCLUDE_FCDPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class FCDPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "org.osmocom.sdr.samplesource.fcd")

public:
	explicit FCDPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	SampleSourceDevices enumSampleSources();
	PluginGUI* createSampleSource(const QString& sourceName, const QByteArray& address);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_FCDPLUGIN_H
