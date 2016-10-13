#include <QDockWidget>
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "mainwindow.h"

MessageQueue* PluginAPI::getMainWindowMessageQueue()
{
	return m_mainWindow->getInputMessageQueue();
}

void PluginAPI::registerRxChannel(const QString& channelName, PluginInterface* plugin)
{
	m_pluginManager->registerRxChannel(channelName, plugin);
}

void PluginAPI::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleSource(sourceName, plugin);
}

PluginAPI::ChannelRegistrations *PluginAPI::getRxChannelRegistrations()
{
    return m_pluginManager->getRxChannelRegistrations();
}


PluginAPI::PluginAPI(PluginManager* pluginManager, MainWindow* mainWindow) :
	QObject(mainWindow),
	m_pluginManager(pluginManager),
	m_mainWindow(mainWindow)
{
}

PluginAPI::~PluginAPI()
{
}
