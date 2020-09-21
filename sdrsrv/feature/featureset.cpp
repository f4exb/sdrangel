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

#include <QList>

#include "plugin/pluginapi.h"
#include "feature/feature.h"
#include "featureset.h"

FeatureSet::FeatureSet(int tabIndex)
{
    m_featureTabIndex = tabIndex;
}

FeatureSet::~FeatureSet()
{
}

void FeatureSet::addFeature(int selectedFeatureIndex, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter)
{
    PluginAPI::FeatureRegistrations *featureRegistrations = pluginAPI->getFeatureRegistrations(); // Available feature plugins
    Feature *feature = featureRegistrations->at(selectedFeatureIndex).m_plugin->createFeature(apiAdapter);
    QString featureName;
    feature->getIdentifier(featureName);
    m_featureInstanceRegistrations.append(FeatureInstanceRegistration(featureName, feature));
    renameFeatureInstances();
}

void FeatureSet::removeFeatureInstance(Feature* feature)
{
    for (FeatureInstanceRegistrations::iterator it = m_featureInstanceRegistrations.begin(); it != m_featureInstanceRegistrations.end(); ++it)
    {
        if (it->m_feature == feature)
        {
            m_featureInstanceRegistrations.erase(it);
            break;
        }
    }

    renameFeatureInstances();
}

void FeatureSet::renameFeatureInstances()
{
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++) {
        m_featureInstanceRegistrations[i].m_feature->setName(QString("%1:%2").arg(m_featureInstanceRegistrations[i].m_featureName).arg(i));
    }
}

void FeatureSet::freeFeatures()
{
    for(int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureSet::freeFeatures: destroying feature [%s]", qPrintable(m_featureInstanceRegistrations[i].m_featureName));
        m_featureInstanceRegistrations[i].m_feature->destroy();
    }
}

void FeatureSet::deleteFeature(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count()))
    {
        qDebug("FeatureSet::deleteFeature: delete feature [%s] at %d",
                qPrintable(m_featureInstanceRegistrations[featureIndex].m_featureName),
                featureIndex);
        m_featureInstanceRegistrations[featureIndex].m_feature->destroy();
        m_featureInstanceRegistrations.removeAt(featureIndex);
        renameFeatureInstances();
    }
}

const Feature *FeatureSet::getFeatureAt(int featureIndex) const
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex].m_feature;
    } else{
        return nullptr;
    }
}

Feature *FeatureSet::getFeatureAt(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex].m_feature;
    } else{
        return nullptr;
    }
}
