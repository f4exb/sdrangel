#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"

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

void PluginAPI::registerTxChannel(const QString& channelName, PluginInterface* plugin)
{
	m_pluginManager->registerTxChannel(channelName, plugin);
}

void PluginAPI::registerSampleSink(const QString& sinkName, PluginInterface* plugin)
{
	m_pluginManager->registerSampleSink(sinkName, plugin);
}

PluginAPI::ChannelRegistrations *PluginAPI::getTxChannelRegistrations()
{
    return m_pluginManager->getTxChannelRegistrations();
}

PluginAPI::PluginAPI(PluginManager* pluginManager) :
	m_pluginManager(pluginManager)
{
}

PluginAPI::~PluginAPI()
{
}
