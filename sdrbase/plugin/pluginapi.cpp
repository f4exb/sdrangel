#include <QDockWidget>
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "mainwindow.h"
#include "dsp/dspengine.h"

QDockWidget* PluginAPI::createMainWindowDock(Qt::DockWidgetArea dockWidgetArea, const QString& title)
{
	QDockWidget* dock = new QDockWidget(title, m_mainWindow);
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	dock->setAttribute(Qt::WA_DeleteOnClose);
	m_mainWindow->addDockWidget(dockWidgetArea, dock);
	m_mainWindow->addViewAction(dock->toggleViewAction());
	return dock;
}

MessageQueue* PluginAPI::getMainWindowMessageQueue()
{
	return m_mainWindow->getInputMessageQueue();
}

void PluginAPI::setInputGUI(QWidget* inputGUI)
{
	m_mainWindow->setInputGUI(inputGUI);
}

void PluginAPI::registerChannel(const QString& channelName, PluginInterface* plugin, QAction* action)
{
	m_pluginManager->registerChannel(channelName, plugin, action);
}

void PluginAPI::registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI)
{
	m_pluginManager->registerChannelInstance(channelName, pluginGUI);
}

void PluginAPI::addChannelRollup(QWidget* pluginGUI)
{
	m_pluginManager->addChannelRollup(pluginGUI);
}

void PluginAPI::removeChannelInstance(PluginGUI* pluginGUI)
{
	m_pluginManager->removeChannelInstance(pluginGUI);
}

void PluginAPI::addChannelMarker(ChannelMarker* channelMarker)
{
	m_mainWindow->addChannelMarker(channelMarker);
}

void PluginAPI::removeChannelMarker(ChannelMarker* channelMarker)
{
	m_mainWindow->removeChannelMarker(channelMarker);
}

void PluginAPI::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleSource(sourceName, plugin);
}

void PluginAPI::addThreadedSink(ThreadedSampleSink* sink)
{
    m_pluginManager->addThreadedSink(sink);
}

void PluginAPI::removeThreadedSink(ThreadedSampleSink* sink)
{
    m_pluginManager->removeThreadedSink(sink);
}

PluginAPI::PluginAPI(PluginManager* pluginManager, MainWindow* mainWindow) :
	QObject(mainWindow),
	m_pluginManager(pluginManager),
	m_mainWindow(mainWindow)
{
}
