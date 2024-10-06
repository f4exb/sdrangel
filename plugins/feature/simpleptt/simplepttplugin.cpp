///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
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
#include "simplepttgui.h"
#endif
#include "simpleptt.h"
#include "simplepttplugin.h"
#include "simplepttwebapiadapter.h"

const PluginDescriptor SimplePTTPlugin::m_pluginDescriptor = {
    SimplePTT::m_featureId,
	QStringLiteral("Simple PTT"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

SimplePTTPlugin::SimplePTTPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& SimplePTTPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void SimplePTTPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register Simple PTT feature
	m_pluginAPI->registerFeature(SimplePTT::m_featureIdURI, SimplePTT::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* SimplePTTPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	(void) featureUISet;
	(void) feature;
    return nullptr;
}
#else
FeatureGUI* SimplePTTPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	return SimplePTTGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* SimplePTTPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new SimplePTT(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* SimplePTTPlugin::createFeatureWebAPIAdapter() const
{
	return new SimplePTTWebAPIAdapter();
}
