///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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
#include "mapgui.h"
#endif
#include "map.h"
#include "mapplugin.h"
#include "mapwebapiadapter.h"

const PluginDescriptor MapPlugin::m_pluginDescriptor = {
    Map::m_featureId,
    QStringLiteral("Map"),
    QStringLiteral("7.22.2"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

MapPlugin::MapPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& MapPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void MapPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(Map::m_featureIdURI, Map::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* MapPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* MapPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return MapGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* MapPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new Map(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* MapPlugin::createFeatureWebAPIAdapter() const
{
    return new MapWebAPIAdapter();
}
