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

#include "gui/workspace.h"
#include "plugin/pluginapi.h"
#include "settings/featuresetpreset.h"
#include "feature/featureutils.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "feature/featuregui.h"

#include "featureuiset.h"

FeatureUISet::FeatureUISet(int tabIndex, FeatureSet *featureSet)
{
    m_featureTabIndex = tabIndex;
    m_featureSet = featureSet;
}

FeatureUISet::~FeatureUISet()
{
    freeFeatures();
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
        qDebug("FeatureUISet::freeFeatures: destroying feature [%s]",
            qPrintable(m_featureInstanceRegistrations.at(i).m_feature->getURI())
        );
        m_featureInstanceRegistrations.at(i).m_gui->destroy();
        m_featureInstanceRegistrations.at(i).m_feature->destroy();
    }

    m_featureInstanceRegistrations.clear();
    m_featureSet->clearFeatures();
}

void FeatureUISet::deleteFeature(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count()))
    {
        qDebug("FeatureUISet::deleteFeature: delete feature [%s] at %d",
            qPrintable(m_featureInstanceRegistrations.at(featureIndex).m_feature->getURI()),
            featureIndex
        );
        m_featureInstanceRegistrations.at(featureIndex).m_gui->destroy();
        m_featureInstanceRegistrations.at(featureIndex).m_feature->destroy();
        m_featureInstanceRegistrations.removeAt(featureIndex);
        m_featureSet->removeFeatureInstanceAt(featureIndex);
    }

    // Renumerate
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++) {
        m_featureInstanceRegistrations.at(i).m_gui->setIndex(i);
    }
}

const Feature *FeatureUISet::getFeatureAt(int featureIndex) const
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations.at(featureIndex).m_feature;
    } else{
        return nullptr;
    }
}

Feature *FeatureUISet::getFeatureAt(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations.at(featureIndex).m_feature;
    } else{
        return nullptr;
    }
}

const FeatureGUI *FeatureUISet::getFeatureGuiAt(int featureIndex) const
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations.at(featureIndex).m_gui;
    } else{
        return nullptr;
    }
}

FeatureGUI *FeatureUISet::getFeatureGuiAt(int featureIndex)
{
    if ((featureIndex >= 0) && (featureIndex < m_featureInstanceRegistrations.count())) {
        return m_featureInstanceRegistrations.at(featureIndex).m_gui;
    } else{
        return nullptr;
    }
}

void FeatureUISet::loadFeatureSetSettings(
    const FeatureSetPreset *preset,
    PluginAPI *pluginAPI,
    WebAPIAdapterInterface *apiAdapter,
    QList<Workspace*> *workspaces,
    Workspace *currentWorkspace
)
{
    qDebug("FeatureUISet::loadFeatureSetSettings: Loading preset [%s | %s]",
        qPrintable(preset->getGroup()),
        qPrintable(preset->getDescription())
    );

    // Available feature plugins
    PluginAPI::FeatureRegistrations *featureRegistrations = pluginAPI->getFeatureRegistrations();

    // copy currently open features and clear list
    FeatureInstanceRegistrations openFeatures = m_featureInstanceRegistrations;
    m_featureInstanceRegistrations.clear();
    m_featureSet->clearFeatures();

    for (int i = 0; i < openFeatures.count(); i++)
    {
        qDebug("FeatureUISet::loadFeatureSetSettings: destroying old feature [%s]",
            qPrintable(openFeatures.at(i).m_feature->getURI())
        );
        openFeatures[i].m_feature->destroy();
        openFeatures[i].m_gui->destroy();
    }

    qDebug("FeatureUISet::loadFeatureSetSettings: %d feature(s) in preset", preset->getFeatureCount());

    for (int i = 0; i < preset->getFeatureCount(); i++)
    {
        const FeatureSetPreset::FeatureConfig& featureConfig = preset->getFeatureConfig(i);
        FeatureGUI *featureGUI = nullptr;
        Feature *feature = nullptr;

        // create feature instance

        for(int i = 0; i < featureRegistrations->count(); i++)
        {
            if (FeatureUtils::compareFeatureURIs((*featureRegistrations)[i].m_featureIdURI, featureConfig.m_featureIdURI))
            {
                qDebug("FeatureUISet::loadFeatureSetSettings: creating new feature [%s] from config [%s]",
                    qPrintable((*featureRegistrations)[i].m_featureIdURI),
                    qPrintable(featureConfig.m_featureIdURI)
                );
                PluginInterface *pluginInterface = (*featureRegistrations)[i].m_plugin;
                feature = pluginInterface->createFeature(apiAdapter);
                featureGUI = pluginInterface->createFeatureGUI(this, feature);
                registerFeatureInstance(featureGUI, feature);
                featureGUI->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
                break;
            }
        }

        if (featureGUI)
        {
            qDebug("FeatureUISet::loadFeatureSetSettings: deserializing feature [%s]",
                qPrintable(featureConfig.m_featureIdURI));
            featureGUI->deserialize(featureConfig.m_config);
            featureGUI->setIndex(feature->getIndexInFeatureSet());
            int originalWorkspaceIndex = featureGUI->getWorkspaceIndex();

            if (workspaces && (workspaces->size() > 0) && (originalWorkspaceIndex < workspaces->size())) // restore in original workspace
            {
                (*workspaces)[originalWorkspaceIndex]->addToMdiArea((QMdiSubWindow*) featureGUI);
                featureGUI->restoreGeometry(featureGUI->getGeometryBytes());
                featureGUI->getRollupContents()->arrangeRollups();
            }
            else if (currentWorkspace) // restore in current workspace
            {
                featureGUI->setWorkspaceIndex(currentWorkspace->getIndex());
                currentWorkspace->addToMdiArea((QMdiSubWindow*) featureGUI);
                featureGUI->restoreGeometry(featureGUI->getGeometryBytes());
                featureGUI->getRollupContents()->arrangeRollups();
            }
        }
    }
}

void FeatureUISet::saveFeatureSetSettings(FeatureSetPreset *preset)
{
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++)
    {
        qDebug("FeatureUISet::saveFeatureSetSettings: saving feature [%s]",
            qPrintable(m_featureInstanceRegistrations.at(i).m_feature->getURI())
        );
        FeatureGUI *featureGUI = m_featureInstanceRegistrations.at(i).m_gui;
        featureGUI->setGeometryBytes(featureGUI->saveGeometry());
        preset->addFeature(
            m_featureInstanceRegistrations.at(i).m_feature->getURI(),
            m_featureInstanceRegistrations.at(i).m_gui->serialize()
        );
    }
}


void FeatureUISet::handleClosingFeatureGUI(FeatureGUI *featureGUI)
{
    for (FeatureInstanceRegistrations::iterator it = m_featureInstanceRegistrations.begin(); it != m_featureInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == featureGUI)
        {
            Feature *feature = it->m_feature;
            m_featureSet->removeFeatureInstance(feature);
            QObject::connect(
                featureGUI,
                &FeatureGUI::destroyed,
                this,
                [this, feature](){ this->handleDeleteFeature(feature); }
            );
            m_featureInstanceRegistrations.erase(it);
            break;
        }
    }

    // Renumerate
    for (int i = 0; i < m_featureInstanceRegistrations.count(); i++) {
        m_featureInstanceRegistrations.at(i).m_gui->setIndex(i);
    }
}

void FeatureUISet::handleDeleteFeature(Feature *feature)
{
    feature->destroy();
}
