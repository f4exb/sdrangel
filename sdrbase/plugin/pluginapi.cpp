#include <QDockWidget>
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "mainwindow.h"

// This was used in Tetra demod plugin which is not part of the build anymore
//QDockWidget* PluginAPI::createMainWindowDock(Qt::DockWidgetArea dockWidgetArea, const QString& title)
//{
//	QDockWidget* dock = new QDockWidget(title, m_mainWindow);
//	dock->setAllowedAreas(Qt::AllDockWidgetAreas);
//	dock->setAttribute(Qt::WA_DeleteOnClose);
//	m_mainWindow->addDockWidget(dockWidgetArea, dock);
//	m_mainWindow->addViewAction(dock->toggleViewAction());
//	return dock;
//}

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
    m_pluginManager->addChannelMarker(channelMarker);
}

void PluginAPI::removeChannelMarker(ChannelMarker* channelMarker)
{
    m_pluginManager->removeChannelMarker(channelMarker);
}

void PluginAPI::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleSource(sourceName, plugin);
}

//void PluginAPI::addSink(SampleSink* sink)
//{
//    m_pluginManager->addSink(sink);
//}

void PluginAPI::removeSink(SampleSink* sink)
{
    m_pluginManager->removeSink(sink);
}

void PluginAPI::addThreadedSink(ThreadedSampleSink* sink)
{
    m_pluginManager->addThreadedSink(sink);
}

void PluginAPI::removeThreadedSink(ThreadedSampleSink* sink)
{
    m_pluginManager->removeThreadedSink(sink);
}

void PluginAPI::setSource(SampleSource* source)
{
    m_pluginManager->setSource(source);
}

bool PluginAPI::initAcquisition()
{
    return m_pluginManager->initAcquisition();
}

bool PluginAPI::startAcquisition()
{
    return m_pluginManager->startAcquisition();
}

void PluginAPI::stopAcquistion()
{
    m_pluginManager->stopAcquistion();
}

DSPDeviceEngine::State PluginAPI::state() const
{
    return m_pluginManager->state();
}

QString PluginAPI::errorMessage()
{
    return m_pluginManager->errorMessage();
}

uint PluginAPI::getDeviceUID() const
{
    return m_pluginManager->getDeviceUID();
}

MessageQueue *PluginAPI::getDeviceInputMessageQueue()
{
    return m_pluginManager->getDeviceInputMessageQueue();
}

MessageQueue *PluginAPI::getDeviceOutputMessageQueue()
{
    return m_pluginManager->getDeviceOutputMessageQueue();
}

void PluginAPI::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
    m_pluginManager->configureCorrections(dcOffsetCorrection, iqImbalanceCorrection);
}

GLSpectrum *PluginAPI::getSpectrum()
{
    return m_pluginManager->getSpectrum();
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
