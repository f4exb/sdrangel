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

#ifndef INCLUDE_FEATURE_MORSEDECODERPLUGIN_H_
#define INCLUDE_FEATURE_MORSEDECODERPLUGIN_H_

#include <QObject>
#include "plugin/plugininterface.h"

class FeatureGUI;
class WebAPIAdapterInterface;

class MorseDecoderPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.feature.morsedecoder")

public:
	explicit MorseDecoderPlugin(QObject* parent = nullptr);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual FeatureGUI* createFeatureGUI(FeatureUISet *featureUISet, Feature *feature) const;
	virtual Feature* createFeature(WebAPIAdapterInterface *webAPIAdapterInterface) const;
	virtual FeatureWebAPIAdapter* createFeatureWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};


#endif // INCLUDE_FEATURE_MORSEDECODERPLUGIN_H_
