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

#include "gui/featurewindow.h"
#include "plugin/pluginapi.h"
#include "settings/featuresetpreset.h"
#include "feature/featureutils.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "feature/featuregui.h"

#include "featureuiset.h"

FeatureUISet::FeatureUISet(int tabIndex, FeatureSet *featureSet)
{
    m_featureWindow = new FeatureWindow;
    m_featureTabIndex = tabIndex;
    m_featureSet = featureSet;
}

FeatureUISet::~FeatureUISet()
{
    freeFeatures();
    delete m_featureWindow;
}

void FeatureUISet::addRollupWidget(QWidget *widget)
{
    m_featureWindow->addRollupWidget(widget);
}

void FeatureUISet::registerFeatureInstance(FeatureGUI* featureGUI, Feature *feature)
{
    m_featureInstanceRegistrations.append(FeatureInstanceRegistration(featureGUI, feature));
    m_featureSet->addFeatureInstance(feature);
    QObject::connect(
        featureGUI,
        &FeatureGUI::closing,
        this,
        [=](){ this->handleClosingFeatureGUI(featureGUI); },
        Qt::QueuedConnection
    );
}

// sort by name
bool FeatureUISet::FeatureInstanceRegistration::operator<(const FeatureInstanceRegistration& other) const
{
    if (m_feature && other.m_feature) {
        return m_feature->getName() < other.m_feature->getName();
    } else {
        return false;
    }
}

void FeatureUISet::freeFeatures()
{
    for(int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureUISet::freeFeatures: destroying feature [%s]", qPrintable(m_featureInstanceRegistrations[i].m_feature->getURI()));
        m_featureInstanceRegistrations[i].m_feature->destroy();
        m_featureInstanceRegistrations[i].m_gui->destroy();
    }

    m_featureSet->clearFeatures();
}

void FeatureUISet::deleteFeature(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count()))
    {
        qDebug("FeatureUISet::deleteFeature: delete feature [%s] at %d",
                qPrintable(m_featureInstanceRegistrations[featureIndex].m_feature->getURI()),
                featureIndex);
        m_featureInstanceRegistrations[featureIndex].m_feature->destroy();
        m_featureInstanceRegistrations[featureIndex].m_gui->destroy();
        m_featureSet->removeFeatureInstanceAt(featureIndex);
    }
}

const Feature *FeatureUISet::getFeatureAt(int featureIndex) const
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex].m_feature;
    } else{
        return nullptr;
    }
}

Feature *FeatureUISet::getFeatureAt(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations[featureIndex].m_feature;
    } else{
        return nullptr;
    }
}

void FeatureUISet::loadFeatureSetSettings(const FeatureSetPreset *preset, PluginAPI *pluginAPI, WebAPIAdapterInterface *apiAdapter)
{
    qDebug("FeatureUISet::loadFeatureSetSettings: Loading preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

    // Available feature plugins
    PluginAPI::FeatureRegistrations *featureRegistrations = pluginAPI->getFeatureRegistrations();

    // copy currently open features and clear list
    FeatureInstanceRegistrations openFeatures = m_featureInstanceRegistrations;
    m_featureInstanceRegistrations.clear();
    m_featureSet->clearFeatures();

    for (int i = 0; i < openFeatures.count(); i++)
    {
        qDebug("FeatureUISet::loadFeatureSetSettings: destroying old feature [%s]", qPrintable(openFeatures[i].m_feature->getURI()));
        openFeatures[i].m_feature->destroy();
        openFeatures[i].m_gui->destroy();
    }

    qDebug("FeatureUISet::loadFeatureSetSettings: %d feature(s) in preset", preset->getFeatureCount());

    for (int i = 0; i < preset->getFeatureCount(); i++)
    {
        const FeatureSetPreset::FeatureConfig& featureConfig = preset->getFeatureConfig(i);
        FeatureGUI *featureGUI = nullptr;

        // create feature instance

        for(int i = 0; i < featureRegistrations->count(); i++)
        {
            if (FeatureUtils::compareFeatureURIs((*featureRegistrations)[i].m_featureIdURI, featureConfig.m_featureIdURI))
            {
                qDebug("FeatureUISet::loadFeatureSetSettings: creating new feature [%s] from config [%s]",
                        qPrintable((*featureRegistrations)[i].m_featureIdURI),
                        qPrintable(featureConfig.m_featureIdURI));
                Feature *feature =
                        (*featureRegistrations)[i].m_plugin->createFeature(apiAdapter);
                featureGUI =
                        (*featureRegistrations)[i].m_plugin->createFeatureGUI(this, feature);
                registerFeatureInstance(featureGUI, feature);
                break;
            }
        }

        if (featureGUI)
        {
            qDebug("FeatureUISet::loadFeatureSetSettings: deserializing feature [%s]", qPrintable(featureConfig.m_featureIdURI));
            featureGUI->deserialize(featureConfig.m_config);
        }
    }
}

void FeatureUISet::saveFeatureSetSettings(FeatureSetPreset *preset)
{
    std::sort(m_featureInstanceRegistrations.begin(), m_featureInstanceRegistrations.end()); // sort by increasing delta frequency and type

    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureUISet::saveFeatureSetSettings: saving feature [%s]", qPrintable(m_featureInstanceRegistrations[i].m_feature->getURI()));
        preset->addFeature(m_featureInstanceRegistrations[i].m_feature->getURI(), m_featureInstanceRegistrations[i].m_gui->serialize());
    }
}


void FeatureUISet::handleClosingFeatureGUI(FeatureGUI *featureGUI)
{
    for (FeatureInstanceRegistrations::iterator it = m_featureInstanceRegistrations.begin(); it != m_featureInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == featureGUI)
        {
            m_featureSet->removeFeatureInstance(it->m_feature);
            it->m_feature->destroy();
            m_featureInstanceRegistrations.erase(it);
            break;
        }
    }
}
