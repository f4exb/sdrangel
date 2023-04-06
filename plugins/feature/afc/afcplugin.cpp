///////////////////////////////////////////////////////////////////////////////////
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
#include "afcgui.h"
#endif
#include "afc.h"
#include "afcplugin.h"
#include "afcwebapiadapter.h"

const PluginDescriptor AFCPlugin::m_pluginDescriptor = {
    AFC::m_featureId,
	QStringLiteral("AFC"),
    QStringLiteral("7.13.0"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

AFCPlugin::AFCPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& AFCPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AFCPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register AFC feature
	m_pluginAPI->registerFeature(AFC::m_featureIdURI, AFC::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* AFCPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	(void) featureUISet;
	(void) feature;
    return nullptr;
}
#else
FeatureGUI* AFCPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	return AFCGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* AFCPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new AFC(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* AFCPlugin::createFeatureWebAPIAdapter() const
{
	return new AFCWebAPIAdapter();
}
