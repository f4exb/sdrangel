#ifndef INCLUDE_FCDPROPLUSPLUGIN_H
#define INCLUDE_FCDPROPLUSPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

#define FCDPROPLUS_DEVICE_TYPE_ID "sdrangel.samplesource.fcdproplus"

class FCDProPlusPlugin : public QObject, public PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "org.osmocom.sdr.samplesource.fcdproplus")

public:
	explicit FCDProPlusPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual SampleSourceDevices enumSampleSources();
	virtual PluginGUI* createSampleSourcePluginGUI(const QString& sourceId);

	static const QString m_deviceTypeID;

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_FCDPROPLUSPLUGIN_H
