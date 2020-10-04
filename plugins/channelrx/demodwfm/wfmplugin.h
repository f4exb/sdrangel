#ifndef INCLUDE_WFMPLUGIN_H
#define INCLUDE_WFMPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSink;

class WFMPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.channel.wfmdemod")

public:
	explicit WFMPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual void createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const;
	virtual ChannelGUI* createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_WFMPLUGIN_H
