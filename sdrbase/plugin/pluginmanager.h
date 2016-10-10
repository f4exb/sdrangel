#ifndef INCLUDE_PLUGINMANAGER_H
#define INCLUDE_PLUGINMANAGER_H

#include <QObject>
#include <QDir>
#include "plugin/plugininterface.h"
#include "plugin/pluginapi.h"
#include "util/export.h"

class QComboBox;
class QPluginLoader;
class Preset;
class MainWindow;
class Message;
class MessageQueue;
class DeviceSourceAPI;

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

	explicit PluginManager(MainWindow* mainWindow, QObject* parent = NULL);
	~PluginManager();

	void loadPlugins();
	const Plugins& getPlugins() const { return m_plugins; }

	// Callbacks from the plugins
	void registerChannel(const QString& channelName, PluginInterface* plugin);
	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);

	PluginAPI::ChannelRegistrations *getChannelRegistrations() { return &m_channelRegistrations; }

	void updateSampleSourceDevices();
	void duplicateLocalSampleSourceDevices(uint deviceUID);
	void fillSampleSourceSelector(QComboBox* comboBox, uint deviceUID);

	int selectSampleSourceByIndex(int index, DeviceSourceAPI *deviceAPI);
	int selectFirstSampleSource(const QString& sourceId, DeviceSourceAPI *deviceAPI);
	int selectSampleSourceBySerialOrSequence(const QString& sourceId, const QString& sourceSerial, int sourceSequence, DeviceSourceAPI *deviceAPI);
	void selectSampleSourceByDevice(void *devicePtr, DeviceSourceAPI *deviceAPI);

	void populateChannelComboBox(QComboBox *channels);
	void createChannelInstance(int channelPluginIndex, DeviceSourceAPI *deviceAPI);

private:
	struct SampleSourceRegistration {
		QString m_sourceId;
		PluginInterface* m_plugin;
		SampleSourceRegistration(const QString& sourceId, PluginInterface* plugin) :
			m_sourceId(sourceId),
			m_plugin(plugin)
		{ }
	};

	typedef QList<SampleSourceRegistration> SampleSourceRegistrations;

	struct SampleSourceDevice {
		PluginInterface* m_plugin;
		QString m_displayName;
		QString m_sourceId;
		QString m_sourceSerial;
		int m_sourceSequence;

		SampleSourceDevice(PluginInterface* plugin,
				const QString& displayName,
				const QString& sourceId,
				const QString& sourceSerial,
				int sourceSequence) :
			m_plugin(plugin),
			m_displayName(displayName),
			m_sourceId(sourceId),
			m_sourceSerial(sourceSerial),
			m_sourceSequence(sourceSequence)
		{ }
	};

	typedef QList<SampleSourceDevice> SampleSourceDevices;

	PluginAPI m_pluginAPI;
	MainWindow* m_mainWindow;
	Plugins m_plugins;

	PluginAPI::ChannelRegistrations m_channelRegistrations; //!< Channel plugins register here
	SampleSourceRegistrations m_sampleSourceRegistrations;  //!< Input source plugins (one per device kind) register here
	SampleSourceDevices m_sampleSourceDevices;              //!< Instances of input sources present in the system

	// "Local" sample source device IDs
	static const QString m_sdrDaemonDeviceTypeID;           //!< SDRdaemon source plugin ID
    static const QString m_sdrDaemonFECDeviceTypeID;        //!< SDRdaemon with FEC source plugin ID
    static const QString m_fileSourceDeviceTypeID;          //!< FileSource source plugin ID

//	QString m_sampleSourceId;
//	QString m_sampleSourceSerial;
//	int m_sampleSourceSequence;
//	PluginGUI* m_sampleSourcePluginGUI;

	void loadPlugins(const QDir& dir);

	friend class MainWindow;
};

static inline bool operator<(const PluginManager::Plugin& a, const PluginManager::Plugin& b)
{
	return a.pluginInterface->getPluginDescriptor().displayedName < b.pluginInterface->getPluginDescriptor().displayedName;
}

#endif // INCLUDE_PLUGINMANAGER_H
