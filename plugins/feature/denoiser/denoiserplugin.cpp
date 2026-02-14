///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include "denoisergui.h"
#endif
#include "denoiser.h"
#include "denoiserplugin.h"
#include "denoiserwebapiadapter.h"
const PluginDescriptor DenoiserPlugin::m_pluginDescriptor = {
    Denoiser::m_featureId,
    QStringLiteral("Denoiser"),
    QStringLiteral("7.23.2"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

DenoiserPlugin::DenoiserPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& DenoiserPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void DenoiserPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register RigCtl Server feature
	m_pluginAPI->registerFeature(Denoiser::m_featureIdURI, Denoiser::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* DenoiserPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	(void) featureUISet;
	(void) feature;
    return nullptr;
}
#else
FeatureGUI* DenoiserPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	return DenoiserGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* DenoiserPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new Denoiser(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* DenoiserPlugin::createFeatureWebAPIAdapter() const
{
	return new DenoiserWebAPIAdapter();
}
