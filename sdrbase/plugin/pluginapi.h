#ifndef INCLUDE_PLUGINAPI_H
#define INCLUDE_PLUGINAPI_H

#include <QObject>
#include "util/export.h"

class QString;

class PluginManager;
class PluginInterface;
class MainWindow;
class MessageQueue;
class PluginGUI;

class SDRANGEL_API PluginAPI : public QObject {
	Q_OBJECT

public:
	// MainWindow access
	MessageQueue* getMainWindowMessageQueue();

	// Channel stuff
	void registerChannel(const QString& channelName, PluginInterface* plugin);
	void registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI);
	void removeChannelInstance(PluginGUI* pluginGUI);

	// Sample Source stuff
	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);

	// R/O access to main window
	const MainWindow* getMainWindow() const { return m_mainWindow; }

protected:
	PluginManager* m_pluginManager;
	MainWindow* m_mainWindow;

	PluginAPI(PluginManager* pluginManager, MainWindow* mainWindow);
	~PluginAPI();

	friend class PluginManager;
};

#endif // INCLUDE_PLUGINAPI_H
