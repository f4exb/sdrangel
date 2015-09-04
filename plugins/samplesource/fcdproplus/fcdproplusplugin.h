#ifndef INCLUDE_FCDPROPLUSPLUGIN_H
#define INCLUDE_FCDPROPLUSPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class FCDProPlusPlugin : public QObject, public PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "org.osmocom.sdr.samplesource.fcdproplus")

public:
	explicit FCDProPlusPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	SampleSourceDevices enumSampleSources();
	PluginGUI* createSampleSourcePluginGUI(const QString& sourceName, const QByteArray& address);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_FCDPROPLUSPLUGIN_H
