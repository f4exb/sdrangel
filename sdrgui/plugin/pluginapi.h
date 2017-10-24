#ifndef INCLUDE_PLUGINAPI_H
#define INCLUDE_PLUGINAPI_H

#include <QObject>
#include <QList>

#include "util/export.h"

class QString;

class PluginManager;
class PluginInterface;
class MessageQueue;
class PluginInstanceGUI;

class SDRANGEL_API PluginAPI : public QObject {
	Q_OBJECT

public:
    struct ChannelRegistration
    {
        QString m_channelName;
        PluginInterface* m_plugin;
        ChannelRegistration(const QString& channelName, PluginInterface* plugin) :
            m_channelName(channelName),
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
