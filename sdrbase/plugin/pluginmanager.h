#ifndef INCLUDE_PLUGINMANAGER_H
#define INCLUDE_PLUGINMANAGER_H

#include <QObject>
#include <QDir>
#include "plugin/plugininterface.h"
#include "plugin/pluginapi.h"
#include "util/export.h"

class QAction;
class QComboBox;
class QPluginLoader;
class Preset;
class MainWindow;
class SampleSource;
class Message;
class DSPDeviceEngine;
class ThreadedSampleSink;

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

	explicit PluginManager(MainWindow* mainWindow, DSPDeviceEngine* dspDeviceEngine, QObject* parent = NULL);
	~PluginManager();
	void loadPlugins();

	const Plugins& getPlugins() const { return m_plugins; }

	void registerChannel(const QString& channelName, PluginInterface* plugin, QAction* action);
	void registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI);
	void addChannelRollup(QWidget* pluginGUI);
	void removeChannelInstance(PluginGUI* pluginGUI);

	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);

	void addThreadedSink(ThreadedSampleSink* sink);
	void removeThreadedSink(ThreadedSampleSink* sink);

	void loadSettings(const Preset* preset);
	void loadSourceSettings(const Preset* preset);
	void saveSettings(Preset* preset);
	void saveSourceSettings(Preset* preset);

	void freeAll();

	bool handleMessage(const Message& message);

	void updateSampleSourceDevices();
	void fillSampleSourceSelector(QComboBox* comboBox);
	int selectSampleSourceByIndex(int index);
	int selectFirstSampleSource(const QString& sourceId);
	int selectSampleSourceBySerialOrSequence(const QString& sourceId, const QString& sourceSerial, int sourceSequence);

private:
	struct ChannelRegistration {
		QString m_channelName;
		PluginInterface* m_plugin;
		ChannelRegistration(const QString& channelName, PluginInterface* plugin) :
			m_channelName(channelName),
			m_plugin(plugin)
		{ }
	};
	typedef QList<ChannelRegistration> ChannelRegistrations;

	struct ChannelInstanceRegistration {
		QString m_channelName;
		PluginGUI* m_gui;
		ChannelInstanceRegistration() :
			m_channelName(),
			m_gui(NULL)
		{ }
		ChannelInstanceRegistration(const QString& channelName, PluginGUI* pluginGUI) :
			m_channelName(channelName),
			m_gui(pluginGUI)
		{ }
		bool operator<(const ChannelInstanceRegistration& other) const;
	};
	typedef QList<ChannelInstanceRegistration> ChannelInstanceRegistrations;

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
	DSPDeviceEngine* m_dspDeviceEngine;
	Plugins m_plugins;

	ChannelRegistrations m_channelRegistrations;
	ChannelInstanceRegistrations m_channelInstanceRegistrations;
	SampleSourceRegistrations m_sampleSourceRegistrations;
	SampleSourceDevices m_sampleSourceDevices;

	QString m_sampleSourceId;
	QString m_sampleSourceSerial;
	int m_sampleSourceSequence;
	PluginGUI* m_sampleSourcePluginGUI;

	void loadPlugins(const QDir& dir);
	void renameChannelInstances();
};

static inline bool operator<(const PluginManager::Plugin& a, const PluginManager::Plugin& b)
{
	return a.pluginInterface->getPluginDescriptor().displayedName < b.pluginInterface->getPluginDescriptor().displayedName;
}

#endif // INCLUDE_PLUGINMANAGER_H
