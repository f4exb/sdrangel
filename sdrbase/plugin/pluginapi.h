#ifndef INCLUDE_PLUGINAPI_H
#define INCLUDE_PLUGINAPI_H

#include <QObject>
#include <QList>

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

	// MainWindow access
	MessageQueue* getMainWindowMessageQueue();

	// Channel stuff
	void registerRxChannel(const QString& channelName, PluginInterface* plugin);
	ChannelRegistrations *getRxChannelRegistrations();

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
