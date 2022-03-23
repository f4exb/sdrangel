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

#ifndef SDRBASE_FEATURE_FEATURESET_H_
#define SDRBASE_FEATURE_FEATURESET_H_

#include <QString>
#include <QList>

#include "export.h"

class PluginAPI;
class Feature;
class FeatureSetPreset;
class WebAPIAdapterInterface;

class SDRBASE_API FeatureSet
{
public:
    FeatureSet(int tabIndex);
    ~FeatureSet();

    int getNumberOfFeatures() const { return m_featureInstanceRegistrations.size(); }
    int getIndex() const { return m_featureTabIndex; }
    Feature *addFeature(int selectedFeatureIndex, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter);
    void removeFeatureInstance(Feature* feature);
    void freeFeatures();
    void deleteFeature(int featureIndex);
    const Feature *getFeatureAt(int featureIndex) const;
    Feature *getFeatureAt(int featureIndex);
    void loadFeatureSetSettings(const FeatureSetPreset* preset, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter);
    void saveFeatureSetSettings(FeatureSetPreset* preset);
    // slave mode
    void addFeatureInstance(Feature *feature);
    void removeFeatureInstanceAt(int index);
    void clearFeatures();

private:
    typedef QList<Feature*> FeatureInstanceRegistrations;

    FeatureInstanceRegistrations m_featureInstanceRegistrations;
    int m_featureTabIndex;

    void renameFeatureInstances();
    static bool compareFeatures(Feature *featureA, Feature *featureB);
};

#endif // SDRBASE_FEATURE_FEATURESET_H_
