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
#include "satellitetrackergui.h"
#endif
#include "satellitetracker.h"
#include "satellitetrackerplugin.h"
#include "satellitetrackerwebapiadapter.h"

const PluginDescriptor SatelliteTrackerPlugin::m_pluginDescriptor = {
    SatelliteTracker::m_featureId,
    QStringLiteral("Satellite Tracker"),
    QStringLiteral("7.8.4"),
    QStringLiteral("(c) Jon Beniston, M7RCE and Daniel Warner (SGP4 library)"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

SatelliteTrackerPlugin::SatelliteTrackerPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& SatelliteTrackerPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SatelliteTrackerPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(SatelliteTracker::m_featureIdURI, SatelliteTracker::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* SatelliteTrackerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* SatelliteTrackerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return SatelliteTrackerGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* SatelliteTrackerPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new SatelliteTracker(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* SatelliteTrackerPlugin::createFeatureWebAPIAdapter() const
{
    return new SatelliteTrackerWebAPIAdapter();
}
