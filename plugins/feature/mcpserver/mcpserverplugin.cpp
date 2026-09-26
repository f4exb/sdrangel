///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "mcpservergui.h"
#endif
#include "mcpserver.h"
#include "mcpserverplugin.h"
#include "mcpserverwebapiadapter.h"

const PluginDescriptor MCPServerPlugin::m_pluginDescriptor = {
    MCPServer::m_featureId,
	QStringLiteral("MCP Server"),
    QStringLiteral("7.28.0"),
	QStringLiteral("(c) Jon Beniston, M7RCE"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

MCPServerPlugin::MCPServerPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& MCPServerPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void MCPServerPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register MCP Server feature
	m_pluginAPI->registerFeature(MCPServer::m_featureIdURI, MCPServer::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* MCPServerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	(void) featureUISet;
	(void) feature;
    return nullptr;
}
#else
FeatureGUI* MCPServerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	return MCPServerGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* MCPServerPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new MCPServer(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* MCPServerPlugin::createFeatureWebAPIAdapter() const
{
	return new MCPServerWebAPIAdapter();
}
