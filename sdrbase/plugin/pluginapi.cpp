#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"

void PluginAPI::registerRxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin)
{
	m_pluginManager->registerRxChannel(channelIdURI, channelId, plugin);
}

PluginAPI::ChannelRegistrations *PluginAPI::getRxChannelRegistrations()
{
    return m_pluginManager->getRxChannelRegistrations();
}

void PluginAPI::registerTxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin)
{
	m_pluginManager->registerTxChannel(channelIdURI, channelId, plugin);
}

PluginAPI::ChannelRegistrations *PluginAPI::getTxChannelRegistrations()
{
    return m_pluginManager->getTxChannelRegistrations();
}

void PluginAPI::registerMIMOChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin)
{
	m_pluginManager->registerMIMOChannel(channelIdURI, channelId, plugin);
}

PluginAPI::ChannelRegistrations *PluginAPI::getMIMOChannelRegistrations()
{
    return m_pluginManager->getMIMOChannelRegistrations();
}

void PluginAPI::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleSource(sourceName, plugin);
}

void PluginAPI::registerSampleSink(const QString& sinkName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleSink(sinkName, plugin);
}

void PluginAPI::registerSampleMIMO(const QString& mimoName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleMIMO(mimoName, plugin);
}

PluginAPI::PluginAPI(PluginManager* pluginManager) :
	m_pluginManager(pluginManager)
{
}

PluginAPI::~PluginAPI()
{
}
