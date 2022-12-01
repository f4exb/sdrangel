///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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
#include "aisgui.h"
#endif
#include "ais.h"
#include "aisplugin.h"
#include "aiswebapiadapter.h"

const PluginDescriptor AISPlugin::m_pluginDescriptor = {
    AIS::m_featureId,
    QStringLiteral("AIS"),
    QStringLiteral("7.8.4"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

AISPlugin::AISPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& AISPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void AISPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(AIS::m_featureIdURI, AIS::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* AISPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* AISPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return AISGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* AISPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new AIS(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* AISPlugin::createFeatureWebAPIAdapter() const
{
    return new AISWebAPIAdapter();
}
