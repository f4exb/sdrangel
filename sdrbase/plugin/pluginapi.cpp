#include <QDockWidget>
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "mainwindow.h"

MessageQueue* PluginAPI::getMainWindowMessageQueue()
{
	return m_mainWindow->getInputMessageQueue();
}

void PluginAPI::setInputGUI(QWidget* inputGUI, const QString& sourceDisplayName)
{
    m_pluginManager->setInputGUI(inputGUI, sourceDisplayName);
}

void PluginAPI::registerChannel(const QString& channelName, PluginInterface* plugin)
{
	m_pluginManager->registerChannel(channelName, plugin);
}

void PluginAPI::registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI)
{
	m_pluginManager->registerChannelInstance(channelName, pluginGUI);
}

void PluginAPI::removeChannelInstance(PluginGUI* pluginGUI)
{
	m_pluginManager->removeChannelInstance(pluginGUI);
}

void PluginAPI::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleSource(sourceName, plugin);
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
