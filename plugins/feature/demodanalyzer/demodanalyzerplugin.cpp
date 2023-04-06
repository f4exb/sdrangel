//////////////////////////////////////////////////////////////////////////////////
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
#include "demodanalyzergui.h"
#endif
#include "demodanalyzer.h"
#include "demodanalyzerplugin.h"
#include "demodanalyzerwebapiadapter.h"

const PluginDescriptor DemodAnalyzerPlugin::m_pluginDescriptor = {
    DemodAnalyzer::m_featureId,
	QStringLiteral("Demod Analyzer"),
    QStringLiteral("7.13.0"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

DemodAnalyzerPlugin::DemodAnalyzerPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& DemodAnalyzerPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void DemodAnalyzerPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register RigCtl Server feature
	m_pluginAPI->registerFeature(DemodAnalyzer::m_featureIdURI, DemodAnalyzer::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* DemodAnalyzerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	(void) featureUISet;
	(void) feature;
    return nullptr;
}
#else
FeatureGUI* DemodAnalyzerPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	return DemodAnalyzerGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* DemodAnalyzerPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new DemodAnalyzer(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* DemodAnalyzerPlugin::createFeatureWebAPIAdapter() const
{
	return new DemodAnalyzerWebAPIAdapter();
}
