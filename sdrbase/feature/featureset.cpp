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
#include "feature/featureutils.h"
#include "settings/featuresetpreset.h"
#include "maincore.h"

#include "featureset.h"

FeatureSet::FeatureSet(int tabIndex)
{
    m_featureTabIndex = tabIndex;
}

FeatureSet::~FeatureSet()
{
}

Feature *FeatureSet::addFeature(int selectedFeatureIndex, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter)
{
    PluginAPI::FeatureRegistrations *featureRegistrations = pluginAPI->getFeatureRegistrations(); // Available feature plugins
    Feature *feature = featureRegistrations->at(selectedFeatureIndex).m_plugin->createFeature(apiAdapter);
    QString featureName;
    feature->getIdentifier(featureName);
    m_featureInstanceRegistrations.append(feature);
    MainCore::instance()->addFeatureInstance(this, feature);
    renameFeatureInstances();
    return feature;
}

void FeatureSet::removeFeatureInstance(Feature* feature)
{
    for (FeatureInstanceRegistrations::iterator it = m_featureInstanceRegistrations.begin(); it != m_featureInstanceRegistrations.end(); ++it)
    {
        if (*it == feature)
        {
            m_featureInstanceRegistrations.erase(it);
            feature->setIndexInFeatureSet(-1);
            MainCore::instance()->removeFeatureInstance(feature);
            break;
        }
    }

    renameFeatureInstances();
}

void FeatureSet::renameFeatureInstances()
{
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        m_featureInstanceRegistrations[i]->setName(QString("%1:%2").arg(m_featureInstanceRegistrations[i]->getURI()).arg(i));
        m_featureInstanceRegistrations[i]->setIndexInFeatureSet(i);
    }
}

// sort by name
bool FeatureSet::compareFeatures(Feature *featureA, Feature *featureB)
{
    if (featureA && featureB) {
        return featureA->getName() < featureB->getName();
    } else {
        return false;
    }
}

void FeatureSet::freeFeatures()
{
    for(int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureSet::freeFeatures: destroying feature [%s]", qPrintable(m_featureInstanceRegistrations[i]->getURI()));
        m_featureInstanceRegistrations[i]->destroy();
    }

    MainCore::instance()->clearFeatures(this);
}

void FeatureSet::deleteFeature(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count()))
    {
        qDebug("FeatureSet::deleteFeature: delete feature [%s] at %d",
                qPrintable(m_featureInstanceRegistrations[featureIndex]->getURI()),
                featureIndex);
        m_featureInstanceRegistrations[featureIndex]->destroy();
        m_featureInstanceRegistrations.removeAt(featureIndex);
        MainCore::instance()->removeFeatureInstanceAt(this, featureIndex);
        renameFeatureInstances();
    }
}

const Feature *FeatureSet::getFeatureAt(int featureIndex) const
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex];
    } else{
        return nullptr;
    }
}

Feature *FeatureSet::getFeatureAt(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex];
    } else{
        return nullptr;
    }
}

void FeatureSet::loadFeatureSetSettings(const FeatureSetPreset *preset, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter)
{
    MainCore *mainCore = MainCore::instance();
    qDebug("FeatureSet::loadFeatureSetSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

    // Available feature plugins
    PluginAPI::FeatureRegistrations *featureRegistrations = pluginAPI->getFeatureRegistrations();

    // copy currently open features and clear list
    FeatureInstanceRegistrations openFeatures = m_featureInstanceRegistrations;
    m_featureInstanceRegistrations.clear();
    mainCore->clearFeatures(this);

    for (int i = 0; i < openFeatures.count(); i++)
    {
        qDebug("FeatureSet::loadFeatureSetSettings: destroying old feature [%s]", qPrintable(openFeatures[i]->getURI()));
        openFeatures[i]->destroy();
    }

    qDebug("FeatureSet::loadFeatureSetSettings: %d feature(s) in preset", preset->getFeatureCount());

    for (int i = 0; i < preset->getFeatureCount(); i++)
    {
        const FeatureSetPreset::FeatureConfig& featureConfig = preset->getFeatureConfig(i);
        Feature* reg = nullptr;

        // create feature instance

        for(int i = 0; i < featureRegistrations->count(); i++)
        {
            if (FeatureUtils::compareFeatureURIs((*featureRegistrations)[i].m_featureIdURI, featureConfig.m_featureIdURI))
            {
                qDebug("FeatureSet::loadFeatureSetSettings: creating new feature [%s] from config [%s]",
                        qPrintable((*featureRegistrations)[i].m_featureIdURI),
                        qPrintable(featureConfig.m_featureIdURI));
                Feature *feature =
                        (*featureRegistrations)[i].m_plugin->createFeature(apiAdapter);
                reg = feature;
                m_featureInstanceRegistrations.append(feature);
                mainCore->addFeatureInstance(this, feature);
                break;
            }
        }

        if (reg)
        {
            qDebug("FeatureSet::loadFeatureSetSettings: deserializing feature [%s]", qPrintable(featureConfig.m_featureIdURI));
            reg->deserialize(featureConfig.m_config);
        }
    }

    renameFeatureInstances();
}

void FeatureSet::saveFeatureSetSettings(FeatureSetPreset *preset)
{
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureSet::saveFeatureSetSettings: saving feature [%s]", qPrintable(m_featureInstanceRegistrations[i]->getURI()));
        preset->addFeature(m_featureInstanceRegistrations[i]->getURI(), m_featureInstanceRegistrations[i]->serialize());
    }
}

void FeatureSet::addFeatureInstance(Feature *feature)
{
    m_featureInstanceRegistrations.push_back(feature);
    renameFeatureInstances();
    MainCore::instance()->addFeatureInstance(this, feature);
}

void FeatureSet::removeFeatureInstanceAt(int index)
{
    if (index < m_featureInstanceRegistrations.size())
    {
        m_featureInstanceRegistrations.removeAt(index);
        renameFeatureInstances();
        MainCore::instance()->removeFeatureInstanceAt(this, index);
    }
}

void FeatureSet::clearFeatures()
{
    m_featureInstanceRegistrations.clear();
    MainCore::instance()->clearFeatures(this);
}
