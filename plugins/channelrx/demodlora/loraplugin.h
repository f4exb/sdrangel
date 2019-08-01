#ifndef INCLUDE_LoRaPLUGIN_H
#define INCLUDE_LoRaPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSink;

class LoRaPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.channel.lorademod")

public:
	explicit LoRaPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual PluginInstanceGUI* createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const;
	virtual BasebandSampleSink* createRxChannelBS(DeviceAPI *deviceAPI) const;
	virtual ChannelAPI* createRxChannelCS(DeviceAPI *deviceAPI) const;

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_LoRaPLUGIN_H
