#ifndef INCLUDE_PLUGINMANAGER_H
#define INCLUDE_PLUGINMANAGER_H

#include <stdint.h>
#include <QObject>
#include <QDir>
#include <QList>
#include <QString>

#include "plugin/plugininterface.h"
#include "plugin/pluginapi.h"
#include "util/export.h"

class QComboBox;
class QPluginLoader;
class Preset;
class Message;
class MessageQueue;
class DeviceSourceAPI;
class DeviceSinkAPI;

class SDRANGEL_API PluginManager : public QObject {
	Q_OBJECT

public:
	struct Plugin
	{
		QString filename;
		QPluginLoader* loader;
		PluginInterface* pluginInterface;

		Plugin(const QString& _filename, QPluginLoader* pluginLoader, PluginInterface* _plugin) :
			filename(_filename),
			loader(pluginLoader),
			pluginInterface(_plugin)
		{ }
	};

	typedef QList<Plugin> Plugins;

	explicit PluginManager(QObject* parent = 0);
	~PluginManager();

	PluginAPI *getPluginAPI() { return &m_pluginAPI; }
	void loadPlugins(const QString& pluginsSubDir);
	const Plugins& getPlugins() const { return m_plugins; }

	// Callbacks from the plugins
	void registerRxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin);
	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);
	void registerTxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin);
	void registerSampleSink(const QString& sourceName, PluginInterface* plugin);

	PluginAPI::SamplingDeviceRegistrations& getSourceDeviceRegistrations() { return m_sampleSourceRegistrations; }
	PluginAPI::SamplingDeviceRegistrations& getSinkDeviceRegistrations() { return m_sampleSinkRegistrations; }
	PluginAPI::ChannelRegistrations *getRxChannelRegistrations() { return &m_rxChannelRegistrations; }
	PluginAPI::ChannelRegistrations *getTxChannelRegistrations() { return &m_txChannelRegistrations; }

    void createRxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceSourceAPI *deviceAPI);
    void listRxChannels(QList<QString>& list);

	void createTxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceSinkAPI *deviceAPI);
	void listTxChannels(QList<QString>& list);

	static const QString& getFileSourceDeviceId() { return m_fileSourceDeviceTypeID; }
	static const QString& getFileSinkDeviceId() { return m_fileSinkDeviceTypeID; }

private:
	struct SamplingDevice { //!< This is the device registration
		PluginInterface* m_plugin;
		QString m_displayName;
		QString m_hadrwareId;
		QString m_deviceId;
		QString m_deviceSerial;
		uint32_t m_deviceSequence;

		SamplingDevice(PluginInterface* plugin,
				const QString& displayName,
				const QString& hadrwareId,
				const QString& deviceId,
				const QString& deviceSerial,
				int deviceSequence) :
			m_plugin(plugin),
			m_displayName(displayName),
			m_hadrwareId(hadrwareId),
			m_deviceId(deviceId),
			m_deviceSerial(deviceSerial),
			m_deviceSequence(deviceSequence)
		{ }
	};

	typedef QList<SamplingDevice> SamplingDevices;

	PluginAPI m_pluginAPI;
	Plugins m_plugins;

	PluginAPI::ChannelRegistrations m_rxChannelRegistrations;           //!< Channel plugins register here
	PluginAPI::SamplingDeviceRegistrations m_sampleSourceRegistrations; //!< Input source plugins (one per device kind) register here

	PluginAPI::ChannelRegistrations m_txChannelRegistrations;         //!< Channel plugins register here
	PluginAPI::SamplingDeviceRegistrations m_sampleSinkRegistrations; //!< Output sink plugins (one per device kind) register here

	// "Local" sample source device IDs
    static const QString m_sdrDaemonSourceHardwareID;   //!< SDRdaemon source hardware ID
    static const QString m_sdrDaemonSourceDeviceTypeID; //!< SDRdaemon source plugin ID
    static const QString m_fileSourceHardwareID;        //!< FileSource source hardware ID
    static const QString m_fileSourceDeviceTypeID;      //!< FileSource source plugin ID

    // "Local" sample sink device IDs
    static const QString m_sdrDaemonSinkHardwareID;   //!< SDRdaemon source hardware ID
    static const QString m_sdrDaemonSinkDeviceTypeID; //!< SDRdaemon source plugin ID
    static const QString m_fileSinkHardwareID;        //!< FileSource source hardware ID
    static const QString m_fileSinkDeviceTypeID;      //!< FileSink sink plugin ID

	void loadPluginsDir(const QDir& dir);
};

static inline bool operator<(const PluginManager::Plugin& a, const PluginManager::Plugin& b)
{
	return a.pluginInterface->getPluginDescriptor().displayedName < b.pluginInterface->getPluginDescriptor().displayedName;
}

#endif // INCLUDE_PLUGINMANAGER_H
