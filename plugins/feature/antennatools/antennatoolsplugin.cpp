///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include "antennatoolsgui.h"
#endif
#include "antennatools.h"
#include "antennatoolsplugin.h"
#include "antennatoolswebapiadapter.h"

const PluginDescriptor AntennaToolsPlugin::m_pluginDescriptor = {
    AntennaTools::m_featureId,
    QStringLiteral("Antenna Tools"),
    QStringLiteral("7.8.4"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

AntennaToolsPlugin::AntennaToolsPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& AntennaToolsPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void AntennaToolsPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(AntennaTools::m_featureIdURI, AntennaTools::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* AntennaToolsPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* AntennaToolsPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return AntennaToolsGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* AntennaToolsPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new AntennaTools(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* AntennaToolsPlugin::createFeatureWebAPIAdapter() const
{
    return new AntennaToolsWebAPIAdapter();
}
