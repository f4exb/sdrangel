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
#include "startrackergui.h"
#endif
#include "startracker.h"
#include "startrackerplugin.h"
#include "startrackerwebapiadapter.h"

const PluginDescriptor StarTrackerPlugin::m_pluginDescriptor = {
    StarTracker::m_featureId,
    QStringLiteral("Star Tracker"),
    QStringLiteral("7.8.4"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

StarTrackerPlugin::StarTrackerPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& StarTrackerPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void StarTrackerPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(StarTracker::m_featureIdURI, StarTracker::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* StarTrackerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* StarTrackerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return StarTrackerGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* StarTrackerPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new StarTracker(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* StarTrackerPlugin::createFeatureWebAPIAdapter() const
{
    return new StarTrackerWebAPIAdapter();
}
