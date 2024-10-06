///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "sidgui.h"
#endif
#include "sid.h"
#include "sidplugin.h"
#include "sidwebapiadapter.h"

const PluginDescriptor SIDPlugin::m_pluginDescriptor = {
    SIDMain::m_featureId,
    QStringLiteral("SID"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

SIDPlugin::SIDPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& SIDPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SIDPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(SIDMain::m_featureIdURI, SIDMain::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* SIDPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* SIDPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return SIDGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* SIDPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new SIDMain(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* SIDPlugin::createFeatureWebAPIAdapter() const
{
    return new SIDWebAPIAdapter();
}
