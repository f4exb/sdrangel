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

#ifndef SDRSRV_FEATURE_FEATURESET_H_
#define SDRSRV_FEATURE_FEATURESET_H_

#include <QString>
#include <QList>

#include "export.h"

class PluginAPI;
class Feature;
class FeatureSetPreset;
class WebAPIAdapterInterface;

class SDRSRV_API FeatureSet
{
public:
    FeatureSet(int tabIndex);
    ~FeatureSet();

    int getNumberOfFeatures() const { return m_featureInstanceRegistrations.size(); }
    void addFeature(int selectedFeatureIndex, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter);
    void removeFeatureInstance(Feature* feature);
    void freeFeatures();
    void deleteFeature(int featureIndex);
    const Feature *getFeatureAt(int featureIndex) const;
    Feature *getFeatureAt(int featureIndex);
    void loadFeatureSetSettings(const FeatureSetPreset* preset, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter);
    void saveFeatureSetSettings(FeatureSetPreset* preset);

private:
    struct FeatureInstanceRegistration
    {
        QString m_featureName;
        Feature* m_feature;

        FeatureInstanceRegistration() :
            m_featureName(),
            m_feature(nullptr)
        { }

        FeatureInstanceRegistration(const QString& featureName, Feature *feature) :
            m_featureName(featureName),
            m_feature(feature)
        { }

        bool operator<(const FeatureInstanceRegistration& other) const;
    };

    typedef QList<FeatureInstanceRegistration> FeatureInstanceRegistrations;

    FeatureInstanceRegistrations m_featureInstanceRegistrations;
    int m_featureTabIndex;

    void renameFeatureInstances();
};

#endif // SDRSRV_FEATURE_FEATURESET_H_
