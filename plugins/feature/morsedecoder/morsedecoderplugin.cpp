///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include "morsedecodergui.h"
#endif
#include "morsedecoder.h"
#include "morsedecoderplugin.h"
#include "morsedecoderwebapiadapter.h"

const PluginDescriptor MorseDecoderPlugin::m_pluginDescriptor = {
    MorseDecoder::m_featureId,
	QStringLiteral("Morse Decoder"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

MorseDecoderPlugin::MorseDecoderPlugin(QObject* parent) :
	QObject(parent),
	m_pluginAPI(nullptr)
{
}

const PluginDescriptor& MorseDecoderPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void MorseDecoderPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register RigCtl Server feature
	m_pluginAPI->registerFeature(MorseDecoder::m_featureIdURI, MorseDecoder::m_featureId, this);
}

#ifdef SERVER_MODE
FeatureGUI* MorseDecoderPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	(void) featureUISet;
	(void) feature;
    return nullptr;
}
#else
FeatureGUI* MorseDecoderPlugin::createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const
{
	return MorseDecoderGUI::create(m_pluginAPI, featureUISet, feature);
}
#endif

Feature* MorseDecoderPlugin::createFeature(WebAPIAdapterInterface* webAPIAdapterInterface) const
{
    return new MorseDecoder(webAPIAdapterInterface);
}

FeatureWebAPIAdapter* MorseDecoderPlugin::createFeatureWebAPIAdapter() const
{
	return new MorseDecoderWebAPIAdapter();
}
