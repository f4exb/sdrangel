///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2017, 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>   //
// Copyright (C) 2020 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
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

void PluginAPI::registerFeature(const QString& featureIdURI, const QString& featureId, PluginInterface* plugin)
{
	m_pluginManager->registerFeature(featureIdURI, featureId, plugin);
}

PluginAPI::FeatureRegistrations *PluginAPI::getFeatureRegistrations()
{
    return m_pluginManager->getFeatureRegistrations();
}

PluginAPI::PluginAPI(PluginManager* pluginManager) :
	m_pluginManager(pluginManager)
{
}

PluginAPI::~PluginAPI()
{
}
