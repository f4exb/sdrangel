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
#include "radiosondegui.h"
#endif
#include "radiosonde.h"
#include "radiosondeplugin.h"
#include "radiosondewebapiadapter.h"

const PluginDescriptor RadiosondePlugin::m_pluginDescriptor = {
    Radiosonde::m_featureId,
    QStringLiteral("Radiosonde"),
    QStringLiteral("7.8.5"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

RadiosondePlugin::RadiosondePlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& RadiosondePlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void RadiosondePlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(Radiosonde::m_featureIdURI, Radiosonde::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* RadiosondePlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* RadiosondePlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return RadiosondeGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* RadiosondePlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new Radiosonde(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* RadiosondePlugin::createFeatureWebAPIAdapter() const
{
    return new RadiosondeWebAPIAdapter();
}
