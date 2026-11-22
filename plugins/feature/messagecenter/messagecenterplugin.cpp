///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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
#include "messagecentergui.h"
#endif

#include "messagecenter.h"
#include "messagecenterplugin.h"
#include "messagecenterwebapiadapter.h"

const PluginDescriptor MessageCenterPlugin::m_pluginDescriptor = {
    MessageCenter::m_featureId,
    QStringLiteral("Message Center"),
    QStringLiteral("8.1.0"),
    QStringLiteral("(c) Vasyl Samoilov"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

MessageCenterPlugin::MessageCenterPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& MessageCenterPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void MessageCenterPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerFeature(MessageCenter::m_featureIdURI, MessageCenter::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* MessageCenterPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* MessageCenterPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return MessageCenterGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* MessageCenterPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new MessageCenter(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* MessageCenterPlugin::createFeatureWebAPIAdapter() const
{
    return new MessageCenterWebAPIAdapter();
}
