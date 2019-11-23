#ifndef INCLUDE_NFMTESTPLUGIN_H
#define INCLUDE_NFMTESTPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSink;

class NFMTestPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.channel.nfmtestdemod")

public:
	explicit NFMTestPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual PluginInstanceGUI* createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const;
    virtual BasebandSampleSink* createRxChannelBS(DeviceAPI *deviceAPI) const;
    virtual ChannelAPI* createRxChannelCS(DeviceAPI *deviceAPI) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_NFMTESTPLUGIN_H
