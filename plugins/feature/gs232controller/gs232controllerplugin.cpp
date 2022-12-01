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
#include "gs232controllergui.h"
#endif
#include "gs232controller.h"
#include "gs232controllerplugin.h"
#include "gs232controllerwebapiadapter.h"

const PluginDescriptor GS232ControllerPlugin::m_pluginDescriptor = {
    GS232Controller::m_featureId,
    QStringLiteral("Rotator Controller"),
    QStringLiteral("7.8.4"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

GS232ControllerPlugin::GS232ControllerPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& GS232ControllerPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void GS232ControllerPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(GS232Controller::m_featureIdURI, GS232Controller::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* GS232ControllerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* GS232ControllerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return GS232ControllerGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* GS232ControllerPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new GS232Controller(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* GS232ControllerPlugin::createFeatureWebAPIAdapter() const
{
    return new GS232ControllerWebAPIAdapter();
}
