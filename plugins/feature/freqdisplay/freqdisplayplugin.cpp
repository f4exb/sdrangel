///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by Copilot                                                          //
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
#include "freqdisplaygui.h"
#endif

#include "freqdisplay.h"
#include "freqdisplayplugin.h"
#include "freqdisplaywebapiadapter.h"

const PluginDescriptor FreqDisplayPlugin::m_pluginDescriptor = {
    FreqDisplay::m_featureId,
    QStringLiteral("Frequency Display"),
    QStringLiteral("7.26  .0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

FreqDisplayPlugin::FreqDisplayPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& FreqDisplayPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void FreqDisplayPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerFeature(FreqDisplay::m_featureIdURI, FreqDisplay::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* FreqDisplayPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* FreqDisplayPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return FreqDisplayGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* FreqDisplayPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new FreqDisplay(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* FreqDisplayPlugin::createFeatureWebAPIAdapter() const
{
    return new FreqDisplayWebAPIAdapter();
}
