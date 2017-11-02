#ifndef INCLUDE_FCDPROPLUGIN_H
#define INCLUDE_FCDPROPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

#define FCDPRO_DEVICE_TYPE_ID "sdrangel.samplesource.fcdpro"

class PluginAPI;

class FCDProPlugin : public QObject, public PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID FCDPRO_DEVICE_TYPE_ID)

public:
	explicit FCDProPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual SamplingDevices enumSampleSources();
	virtual PluginInstanceGUI* createSampleSourcePluginInstanceGUI(
	        const QString& sourceId,
	        QWidget **widget,
	        DeviceUISet *deviceUISet);
	virtual DeviceSampleSource* createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI);

private:
	static const PluginDescriptor m_pluginDescriptor;
};

#endif // INCLUDE_FCDPLUGIN_H
