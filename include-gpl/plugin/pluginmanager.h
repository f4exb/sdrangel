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

class SDRANGELOVE_API PluginManager : public QObject {
	Q_OBJECT

public:
	struct Plugin {
		QString filename;
		QPluginLoader* loader;
		PluginInterface* plugin;
		Plugin(const QString& _filename, QPluginLoader* pluginLoader, PluginInterface* _plugin) :
			filename(_filename),
			loader(pluginLoader),
			plugin(_plugin)
		{ }
	};
	typedef QList<Plugin> Plugins;

	explicit PluginManager(MainWindow* mainWindow, DSPEngine* dspEngine, QObject* parent = NULL);
	~PluginManager();
	void loadPlugins();

	const Plugins& getPlugins() const { return m_plugins; }

	void registerChannel(const QString& channelName, PluginInterface* plugin, QAction* action);
	void registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI);
	void addChannelRollup(QWidget* pluginGUI);
	void removeChannelInstance(PluginGUI* pluginGUI);

	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);

	void loadSettings(const Preset* preset);
	void saveSettings(Preset* preset) const;

	void freeAll();

	bool handleMessage(Message* message);

	void updateSampleSourceDevices();
	void fillSampleSourceSelector(QComboBox* comboBox);
	int selectSampleSource(int index);
	int selectSampleSource(const QString& source);

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
	};
	typedef QList<ChannelInstanceRegistration> ChannelInstanceRegistrations;

	struct SampleSourceRegistration {
		QString m_sourceName;
		PluginInterface* m_plugin;
		SampleSourceRegistration(const QString& sourceName, PluginInterface* plugin) :
			m_sourceName(sourceName),
			m_plugin(plugin)
		{ }
	};
	typedef QList<SampleSourceRegistration> SampleSourceRegistrations;

	struct SampleSourceDevice {
		PluginInterface* m_plugin;
		QString m_displayName;
		QString m_sourceName;
		QByteArray m_address;

		SampleSourceDevice(PluginInterface* plugin, const QString& displayName, const QString& sourceName, const QByteArray& address) :
			m_plugin(plugin),
			m_displayName(displayName),
			m_sourceName(sourceName),
			m_address(address)
		{ }
	};
	typedef QList<SampleSourceDevice> SampleSourceDevices;

	PluginAPI m_pluginAPI;
	MainWindow* m_mainWindow;
	DSPEngine* m_dspEngine;
	Plugins m_plugins;

	ChannelRegistrations m_channelRegistrations;
	ChannelInstanceRegistrations m_channelInstanceRegistrations;
	SampleSourceRegistrations m_sampleSourceRegistrations;
	SampleSourceDevices m_sampleSourceDevices;

	QString m_sampleSource;
	PluginGUI* m_sampleSourceInstance;

	void loadPlugins(const QDir& dir);
	void renameChannelInstances();
};

static inline bool operator<(const PluginManager::Plugin& a, const PluginManager::Plugin& b)
{
	return a.plugin->getPluginDescriptor().displayedName < b.plugin->getPluginDescriptor().displayedName;
}

#endif // INCLUDE_PLUGINMANAGER_H
