///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020-2021 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include "pertestergui.h"
#endif
#include "pertester.h"
#include "pertesterplugin.h"
#include "pertesterwebapiadapter.h"

const PluginDescriptor PERTesterPlugin::m_pluginDescriptor = {
    PERTester::m_featureId,
    QStringLiteral("Packet Error Rate Tester"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

PERTesterPlugin::PERTesterPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& PERTesterPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void PERTesterPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerFeature(PERTester::m_featureIdURI, PERTester::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* PERTesterPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    (void) featureUISet;
    (void) feature;
    return nullptr;
}
#else
FeatureGUI* PERTesterPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
    return PERTesterGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* PERTesterPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new PERTester(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* PERTesterPlugin::createFeatureWebAPIAdapter() const
{
    return new PERTesterWebAPIAdapter();
}
