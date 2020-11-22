#ifndef INCLUDE_RTLSDRPLUGIN_H
#define INCLUDE_RTLSDRPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class PluginAPI;

#define RTLSDR_DEVICE_TYPE_ID "sdrangel.samplesource.rtlsdr"

class RTLSDRPlugin : public QObject, public PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID RTLSDR_DEVICE_TYPE_ID)

public:
	explicit RTLSDRPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual void enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices);
	virtual SamplingDevices enumSampleSources(const OriginDevices& originDevices);
	virtual DeviceGUI* createSampleSourcePluginInstanceGUI(
	        const QString& sourceId,
	        QWidget **widget,
	        DeviceUISet *deviceUISet);
	virtual DeviceSampleSource* createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI);
    virtual DeviceWebAPIAdapter* createDeviceWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;
};

#endif // INCLUDE_RTLSDRPLUGIN_H
