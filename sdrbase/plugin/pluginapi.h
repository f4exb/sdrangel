#ifndef INCLUDE_PLUGINAPI_H
#define INCLUDE_PLUGINAPI_H

#include <QObject>
#include <QList>

#include "util/export.h"
#include "plugin/plugininterface.h"

class QString;

class PluginManager;
class MessageQueue;
class PluginInstanceGUI;

class SDRANGEL_API PluginAPI : public QObject {
	Q_OBJECT

public:
    struct SamplingDeviceRegistration //!< This is the device registration
    {
        QString m_deviceId;
        PluginInterface* m_plugin;
        SamplingDeviceRegistration(const QString& deviceId, PluginInterface* plugin) :
            m_deviceId(deviceId),
            m_plugin(plugin)
        { }
    };

    typedef QList<SamplingDeviceRegistration> SamplingDeviceRegistrations;

    struct ChannelRegistration
    {
        QString m_channelId;       //!< Channel or device type ID
        PluginInterface* m_plugin;
        ChannelRegistration(const QString& channelId, PluginInterface* plugin) :
            m_channelId(channelId),
            m_plugin(plugin)
        { }
    };

    typedef QList<ChannelRegistration> ChannelRegistrations;

	// Rx Channel stuff
	void registerRxChannel(const QString& channelName, PluginInterface* plugin);
	ChannelRegistrations *getRxChannelRegistrations();

	// Tx Channel stuff
	void registerTxChannel(const QString& channelName, PluginInterface* plugin);
	ChannelRegistrations *getTxChannelRegistrations();

	// Sample Source stuff
	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);

	// Sample Sink stuff
	void registerSampleSink(const QString& sinkName, PluginInterface* plugin);

protected:
	PluginManager* m_pluginManager;

	PluginAPI(PluginManager* pluginManager);
	~PluginAPI();

	friend class PluginManager;
};

#endif // INCLUDE_PLUGINAPI_H
